// MAZ Pocket — persisted settings.
//
// Stored in NVS (Preferences), deliberately NOT in LittleFS: when M5Launcher
// flashes MAZ Pocket it uses its own partition table, so a LittleFS partition
// may not exist — but every ESP32 layout has an `nvs` partition. Settings
// therefore survive being launched from any launcher.
#pragma once
#include <stdint.h>

#include <string>

namespace maz {

struct Settings {
    uint8_t  brightness    = 110;  // 0..255, LCD backlight
    uint8_t  volume        = 160;  // 0..255, speaker
    bool     uiSounds      = true;
    uint16_t screenTimeout = 60;    // seconds, 0 = never
    uint8_t  micGain       = 12;    // Mic_Class magnification
    bool     preferSd      = true;  // when an SD is present, write data there

    std::string wifiSsid, wifiPass;
    std::string wifiSsid2, wifiPass2;

    std::string hostAddr;  // MAZ Host — empty means "not configured"
    uint16_t    hostPort = 8787;
    std::string hostToken;
    uint8_t     talkRoute = 1;  // 0 local, 1 auto, 2 cloud
    bool        ttsEnabled = false;
    uint8_t     nudgePollMinutes = 5;
    bool        firstRunComplete = false;

    int8_t   tzMinutesOffset = 0;  // units of 15 min, so +4 == UTC+1h
    uint32_t lastKnownEpoch  = 0;  // clock is roughly right after a cold boot

    void load();
    void save() const;
    void applyToHardware() const;  // brightness + volume, immediately
};

extern Settings Cfg;

}  // namespace maz
