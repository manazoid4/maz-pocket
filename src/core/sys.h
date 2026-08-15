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
    // A card answered on the bus but carried no usable filesystem. Distinct
    // from "no card": an unformatted card silently demotes you to internal
    // flash, which is the volatile backend, and you would never know.
    bool    sdUnreadable = false;
    // Internal LittleFS came up empty because it had to be formatted — which
    // means whatever was stored there is gone. Flashing a different partition
    // table (M5Launcher's vs ours) moves the data region and triggers this.
    bool    internalFormatted = false;

    // network
    bool        wifiOn        = false;
    bool        wifiConnected = false;
    std::string wifiSsid;
    std::string ip;

    // MAZ Host compute endpoint. Never faked: if it is not configured we say
    // so rather than showing a hopeful green dot.
    std::string hostAddr;
    uint16_t    hostPort   = 0;
    bool        hostOnline = false;

    // Agent assurance summary. These are deterministic host facts, never LLM guesses.
    uint8_t     agentsWorking = 0;
    uint8_t     agentsWaiting = 0;
    uint8_t     agentsStale   = 0;
    bool        agentQuestion = false;
    bool        nudgeDue      = false;
    uint32_t    nudgeCheckedAt = 0;

    // Navigation. The chrome draws the back affordance from this, so it stays
    // correct on every screen without each app remembering to say so itself.
    uint8_t navDepth = 1;  // shell stack size; 1 == Home, nothing behind it

    // live activity, surfaced in the status bar
    bool        recording    = false;
    uint32_t    recSeconds   = 0;
    bool        focusRunning = false;
    uint32_t    focusRemain  = 0;  // seconds
    std::string focusLabel;

    // Cardputer ADV runtime health. The board has no PSRAM, so these measured
    // values are more useful than theoretical allocation rules. They are
    // observational only: MAZ Pocket never reboots itself just because a
    // threshold was crossed.
    uint32_t freeHeap           = 0;
    uint32_t minFreeHeap        = 0;
    uint32_t largestFreeBlock   = 0;
    uint32_t mainStackHighWater = 0;
    uint32_t loopMaxMs          = 0;
    bool     memoryPressure     = false;
    bool     uiStallObserved    = false;

    uint32_t uptimeSeconds() const;
};

extern SysState Sys;

}  // namespace maz
