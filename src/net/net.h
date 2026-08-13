// MAZ Pocket — networking.
//
// v0.1 does not need the network for anything, which is exactly why it is
// built now: Wi-Fi credentials, reconnect and a real reachability probe for
// MAZ Host are the unglamorous parts that would otherwise be rushed later,
// when Call starts streaming audio to OpenFlowKit.
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
    bool        known = false;  // matches a saved network
};

void begin();   // radio off until asked — Wi-Fi is the biggest battery cost
void update();  // reconnect backoff, status mirroring into Sys

bool enable(bool on);
bool connectSaved();  // primary, then secondary
bool connect(const std::string& ssid, const std::string& pass);
void disconnect();

bool        connected();
int         rssi();
std::string ip();

std::vector<Ap> scan(uint32_t timeoutMs = 6000);

// MAZ Host reachability. Returns true only if a TCP connection actually
// opened: the status bar never shows a host as online on the strength of a
// saved setting alone.
bool probeHost();

// Time from NTP once we are online, so notes and recordings get real stamps.
bool syncClock();

}  // namespace net
}  // namespace maz
