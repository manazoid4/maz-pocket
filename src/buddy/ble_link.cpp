// Adapted from anthropics/claude-desktop-buddy src/ble_bridge.cpp
// (Copyright 2026 Anthropic, PBC, MIT). See ble_link.h for the change list.
#include "ble_link.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_mac.h>

#include <cstdio>
#include <cstring>

namespace maz {
namespace ble_link {

namespace {

constexpr const char* NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

// Holds a heartbeat snapshot plus headroom. Written from the BLE task, read
// from loop(): single producer / single consumer, so head/tail need no lock.
constexpr size_t RX_CAP = 2048;
uint8_t          rxBuf[RX_CAP];
volatile size_t  rxHead = 0;
volatile size_t  rxTail = 0;

BLEServer*         server  = nullptr;
BLECharacteristic* txChar  = nullptr;
volatile bool      isUp    = false;
volatile bool      isConn  = false;
volatile bool      isSec   = false;
volatile uint32_t  pk      = 0;
volatile uint16_t  mtu     = 23;
char               devName[24] = "Claude-MAZ";

void rxPush(const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const size_t next = (rxHead + 1) % RX_CAP;
        if (next == rxTail) return;  // full: drop, the line parser resyncs on '\n'
        rxBuf[rxHead] = p[i];
        rxHead = next;
    }
}

class RxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        std::string v = c->getValue();
        if (!v.empty()) rxPush(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
};

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override { isConn = true; }
    void onDisconnect(BLEServer*) override {
        isConn = false;
        isSec  = false;
        pk     = 0;
        mtu    = 23;
        BLEDevice::startAdvertising();
    }
    void onMtuChanged(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
        mtu = param->mtu.mtu;
    }
};

// Passkey entry: Pocket is DisplayOnly, the desktop types the 6 digits.
class SecCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() override { return 0; }
    bool     onConfirmPIN(uint32_t) override { return false; }
    bool     onSecurityRequest() override { return true; }
    void     onPassKeyNotify(uint32_t key) override { pk = key; }
    void     onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
        pk    = 0;
        isSec = cmpl.success;
        if (!cmpl.success && server) server->disconnect(server->getConnId());
    }
};

}  // namespace

void begin() {
    if (isUp) return;

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(devName, sizeof(devName), "Claude-MAZ-%02X%02X", mac[4], mac[5]);

    BLEDevice::init(devName);
    BLEDevice::setMTU(517);
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    BLEDevice::setSecurityCallbacks(new SecCallbacks());

    server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    BLEService* svc = server->createService(NUS_SERVICE_UUID);

    // Transcript snippets and tool hints cross this link: encrypted-only
    // characteristics force LE Secure Connections bonding on first access.
    txChar = svc->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    txChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    auto* cccd = new BLE2902();
    cccd->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    txChar->addDescriptor(cccd);

    BLECharacteristic* rxChar = svc->createCharacteristic(
        NUS_RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rxChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    rxChar->setCallbacks(new RxCallbacks());

    svc->start();

    auto* sec = new BLESecurity();
    sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    sec->setCapability(ESP_IO_CAP_OUT);
    sec->setKeySize(16);
    sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    isUp = true;
}

bool        started() { return isUp; }
bool        connected() { return isConn; }
bool        secure() { return isSec; }
uint32_t    passkey() { return pk; }
const char* name() { return devName; }

void clearBonds() {
    if (!isUp) return;
    int n = esp_ble_get_bond_device_num();
    if (n <= 0) return;
    auto* list = static_cast<esp_ble_bond_dev_t*>(malloc(n * sizeof(esp_ble_bond_dev_t)));
    if (!list) return;
    esp_ble_get_bond_device_list(&n, list);
    for (int i = 0; i < n; i++) esp_ble_remove_bond_device(list[i].bd_addr);
    free(list);
}

size_t available() { return (rxHead + RX_CAP - rxTail) % RX_CAP; }

int read() {
    if (rxHead == rxTail) return -1;
    const int b = rxBuf[rxTail];
    rxTail = (rxTail + 1) % RX_CAP;
    return b;
}

size_t write(const uint8_t* data, size_t len) {
    if (!isConn || !txChar) return 0;
    size_t chunk = mtu > 3 ? mtu - 3 : 20;
    if (chunk > 180) chunk = 180;
    size_t sent = 0;
    while (sent < len) {
        size_t n = len - sent;
        if (n > chunk) n = chunk;
        txChar->setValue(const_cast<uint8_t*>(data + sent), n);
        txChar->notify();
        sent += n;
        delay(4);  // let the stack flush before the next notify
    }
    return sent;
}

}  // namespace ble_link
}  // namespace maz
