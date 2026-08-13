// MAZ Pocket — the small pile of state the whole device agrees on.
//
// The status bar, Home and the notification system all need to know "is a
// recording running", "is Focus counting down", "are we online". Passing that
// through constructors would couple every app to every other; a single owned
// snapshot keeps the dependency graph flat and the redraws cheap.
#pragma once
#include <stdint.h>

#include <string>

namespace maz {

enum class Storage : uint8_t { None, Internal, SD };

struct SysState {
    // clock
    uint32_t bootMillis = 0;
    bool     timeValid  = false;  // false until NTP or manual set

    // power
    int  batteryPct       = -1;  // -1 = unknown
    bool charging         = false;
    bool lowBatteryWarned = false;

    // storage
    Storage storage    = Storage::None;
    bool    sdPresent  = false;
    bool    internalFs = false;

    // network
    bool        wifiOn        = false;
    bool        wifiConnected = false;
    std::string wifiSsid;
    std::string ip;

    // MAZ Host (the future OpenFlowKit / MAZos endpoint). Never faked: if it
    // is not configured we say so rather than showing a hopeful green dot.
    std::string hostAddr;
    uint16_t    hostPort   = 0;
    bool        hostOnline = false;

    // live activity, surfaced in the status bar
    bool        recording    = false;
    uint32_t    recSeconds   = 0;
    bool        focusRunning = false;
    uint32_t    focusRemain  = 0;  // seconds
    std::string focusLabel;

    uint32_t uptimeSeconds() const;
};

extern SysState Sys;

}  // namespace maz
