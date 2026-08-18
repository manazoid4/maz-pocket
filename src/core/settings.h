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

    std::string hostAddr;
    uint16_t    hostPort = 8787;
    std::string hostRemoteUrl;
    std::string hostToken;
    // Fresh v0.8 installs prefer CLOUD for Call MAZ as requested. Existing
    // devices keep their persisted choice. A on the Call screen cycles
    // CLOUD -> LOCAL -> AUTO without needing the web settings page.
    uint8_t     talkRoute = 2;
    bool        ttsEnabled = true;
    uint8_t     nudgePollMinutes = 5;
    bool        firstRunComplete = false;

    // Four quick keys are stable action IDs from a fixed list; they are never
    // shell commands. CALL remains quick slot 1 by default.
    bool        fieldMode = false;
    std::string quick1 = "talk";
    std::string quick2 = "braindump";
    std::string quick3 = "laptop";
    std::string quick4 = "beam";

    int8_t   tzMinutesOffset = 0;
    uint32_t lastKnownEpoch  = 0;

    void load();
    void save() const;
    void applyToHardware() const;
};

extern Settings Cfg;

}  // namespace maz
