// MAZ Pocket — networking.
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

namespace maz {
namespace net {

struct Ap {
    std::string ssid;
    int32_t     rssi  = 0;
    bool        open  = false;
    bool        known = false;
};

void begin();
void update();

bool enable(bool on);
bool connectSaved();
bool connect(const std::string& ssid, const std::string& pass);
void disconnect();

bool        connected();
int         rssi();
std::string ip();

std::vector<Ap> scan(uint32_t timeoutMs = 6000);

// v0.5 provisioning fallback. When no saved network works the device exposes
// MAZ-Pocket-Setup so Wi-Fi can always be repaired from the Cardputer or phone.
bool        startSetupAp();
void        stopSetupAp();
bool        setupApActive();
std::string setupIp();

bool probeHost();
bool syncClock();

}  // namespace net
}  // namespace maz
