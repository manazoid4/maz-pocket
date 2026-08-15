// MAZ Pocket — persisted settings.
#pragma once
#include <stdint.h>

#include <string>

namespace maz {

struct Settings {
    uint8_t  brightness    = 110;
    uint8_t  volume        = 160;
    bool     uiSounds      = true;
    uint16_t screenTimeout = 60;
    uint8_t  micGain       = 12;
    bool     preferSd      = true;

    std::string wifiSsid, wifiPass;
    std::string wifiSsid2, wifiPass2;

    // Local MAZ Host is always attempted first. hostRemoteUrl is an optional
    // full HTTPS base URL (normally a Tailscale Funnel) used only when the
    // laptop cannot be reached directly on the LAN.
    std::string hostAddr;
    uint16_t    hostPort = 8787;
    std::string hostRemoteUrl;
    std::string hostToken;
    uint8_t     talkRoute = 1;
    bool        ttsEnabled = true;
    uint8_t     nudgePollMinutes = 5;
    bool        firstRunComplete = false;

    int8_t   tzMinutesOffset = 0;
    uint32_t lastKnownEpoch  = 0;

    void load();
    void save() const;
    void applyToHardware() const;
};

extern Settings Cfg;

}  // namespace maz
