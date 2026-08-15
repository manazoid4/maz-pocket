#include "net.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>

#include "../core/notify.h"
#include "../core/settings.h"
#include "../core/sys.h"

namespace maz {
namespace net {

namespace {
uint32_t gNextRetry    = 0;
uint32_t gBackoff      = 5000;
bool     gWasConnected = false;
}  // namespace

void begin() {
    WiFi.persistent(false);
    Sys.wifiConnected = false;
    Sys.hostAddr      = Cfg.hostAddr;
    Sys.hostPort      = Cfg.hostPort;
    if (Cfg.wifiSsid.empty()) {
        WiFi.mode(WIFI_OFF);
        Sys.wifiOn = false;
        return;
    }

    // Start association without holding up the first screen. update() mirrors
    // the result and applies bounded retry/backoff in the normal event loop.
    WiFi.mode(WIFI_STA);
    Sys.wifiOn   = true;
    Sys.wifiSsid = Cfg.wifiSsid;
    WiFi.begin(Cfg.wifiSsid.c_str(), Cfg.wifiPass.c_str());
    gNextRetry = millis() + 12000;
}

bool connect(const std::string& ssid, const std::string& pass) {
    if (ssid.empty()) return false;
    WiFi.mode(WIFI_STA);
    Sys.wifiOn = true;
    WiFi.begin(ssid.c_str(), pass.c_str());

    const uint32_t deadline = millis() + 12000;
    while (millis() < deadline && WiFi.status() != WL_CONNECTED) {
        delay(120);
    }
    const bool ok = WiFi.status() == WL_CONNECTED;
    if (ok) {
        Sys.wifiConnected = true;
        Sys.wifiSsid      = ssid;
        Sys.ip            = WiFi.localIP().toString().c_str();
        gBackoff          = 5000;
    }
    return ok;
}

bool connectSaved() {
    if (connect(Cfg.wifiSsid, Cfg.wifiPass)) return true;
    if (connect(Cfg.wifiSsid2, Cfg.wifiPass2)) return true;
    return false;
}

bool enable(bool on) {
    if (on) {
        WiFi.mode(WIFI_STA);
        Sys.wifiOn = true;
        return connectSaved();
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Sys.wifiOn        = false;
    Sys.wifiConnected = false;
    Sys.hostOnline    = false;
    Sys.ip.clear();
    return true;
}

void disconnect() {
    WiFi.disconnect();
    Sys.wifiConnected = false;
    Sys.hostOnline    = false;
    Sys.ip.clear();
}

bool        connected() { return WiFi.status() == WL_CONNECTED; }
int         rssi() { return connected() ? WiFi.RSSI() : 0; }
std::string ip() {
    return connected() ? WiFi.localIP().toString().c_str() : "";
}

void update() {
    if (!Sys.wifiOn) return;

    const bool now    = connected();
    Sys.wifiConnected = now;
    if (now) {
        Sys.ip = WiFi.localIP().toString().c_str();
        if (!gWasConnected) {
            // Keep the small v0.2 confirmation the user already relies on,
            // and make the new phone-first control surface discoverable at the
            // exact moment it becomes useful.
            notify::post(Note::Success, "Wi-Fi connected",
                         Sys.wifiSsid + " / mazpocket.local");
            syncClock();
        }
        gWasConnected = true;
        gBackoff      = 5000;
        return;
    }

    if (gWasConnected) {
        notify::post(Note::Warn, "Wi-Fi lost", "retrying automatically");
        gWasConnected  = false;
        Sys.hostOnline = false;
        gNextRetry     = millis() + gBackoff;
    }

    // Exponential backoff to 60s: a Cardputer in a bag should not hammer the
    // radio for an access point that is not there.
    if (millis() >= gNextRetry) {
        if (!connectSaved()) {
            gBackoff   = gBackoff < 60000 ? gBackoff * 2 : 60000;
            gNextRetry = millis() + gBackoff;
        }
    }
}

std::vector<Ap> scan(uint32_t timeoutMs) {
    std::vector<Ap> out;
    WiFi.mode(WIFI_STA);
    Sys.wifiOn = true;

    const int n = WiFi.scanNetworks(false, true, false, timeoutMs / 10);
    for (int i = 0; i < n && i < 24; ++i) {
        Ap a;
        a.ssid  = WiFi.SSID(i).c_str();
        a.rssi  = WiFi.RSSI(i);
        a.open  = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
        a.known = (a.ssid == Cfg.wifiSsid) || (a.ssid == Cfg.wifiSsid2);
        out.push_back(a);
    }
    WiFi.scanDelete();
    return out;
}

bool probeHost() {
    Sys.hostAddr = Cfg.hostAddr;
    Sys.hostPort = Cfg.hostPort;
    if (Cfg.hostAddr.empty() || !connected()) {
        Sys.hostOnline = false;
        return false;
    }
    WiFiClient c;
    c.setTimeout(2);
    const bool ok = c.connect(Cfg.hostAddr.c_str(), Cfg.hostPort);
    c.stop();
    Sys.hostOnline = ok;
    return ok;
}

bool syncClock() {
    if (!connected()) return false;
    configTime(Cfg.tzMinutesOffset * 15 * 60, 0, "pool.ntp.org",
               "time.nist.gov");
    const uint32_t deadline = millis() + 6000;
    while (millis() < deadline) {
        const time_t t = time(nullptr);
        if (t > 1700000000) {  // sane epoch, i.e. NTP actually answered
            Sys.timeValid      = true;
            Cfg.lastKnownEpoch = static_cast<uint32_t>(t);
            Cfg.save();
            return true;
        }
        delay(150);
    }
    return false;
}

}  // namespace net
}  // namespace maz
