// MAZ Pocket — shared lightweight state snapshot.
#pragma once
#include <stdint.h>

#include <string>

namespace maz {

enum class Storage : uint8_t { None, Internal, SD };

struct SysState {
    uint32_t bootMillis = 0;
    bool     timeValid  = false;

    int  batteryPct       = -1;
    bool charging         = false;
    bool lowBatteryWarned = false;

    Storage storage    = Storage::None;
    bool    sdPresent  = false;
    bool    internalFs = false;
    bool    sdUnreadable = false;
    bool    internalFormatted = false;

    bool        wifiOn        = false;
    bool        wifiConnected = false;
    std::string wifiSsid;
    std::string ip;

    std::string hostAddr;
    uint16_t    hostPort   = 0;
    bool        hostOnline = false;

    uint8_t     agentsWorking = 0;
    uint8_t     agentsWaiting = 0;
    uint8_t     agentsStale   = 0;
    bool        agentQuestion = false;
    bool        nudgeDue      = false;
    uint32_t    nudgeCheckedAt = 0;

    uint8_t navDepth = 1;

    bool        recording    = false;
    uint32_t    recSeconds   = 0;
    bool        focusRunning = false;
    uint32_t    focusRemain  = 0;
    std::string focusLabel;

    uint8_t  outboxQueued = 0;
    uint8_t  beamUnread = 0;
    bool     shiftRunning = false;
    uint32_t shiftSeconds = 0;
    bool     fieldMode = false;

    bool     laptopStatusOk = false;
    int      laptopCpuPct = -1;
    int      laptopRamPct = -1;
    int      laptopBatteryPct = -1;
    bool     laptopCharging = false;
    bool     laptopGpuAvailable = false;
    int      laptopGpuPct = -1;
    int      laptopVramUsedMb = 0;
    int      laptopVramTotalMb = 0;
    int      laptopGpuTempC = -1;
    bool     ollamaOnline = false;
    bool     ollamaLoaded = false;
    std::string ollamaModel;
    int      ollamaVramMb = 0;
    int      ollamaContext = 0;
    uint32_t laptopStatusAt = 0;

    static constexpr int WORK_MAX_TRACKS = 4;
    static constexpr int WORK_HISTORY_DAYS = 7;
    bool        workLoaded = false;  // ever received one successful payload
    uint32_t    workReceivedAt = 0;  // millis() of the last successful poll
    uint8_t     workTrackCount = 0;
    std::string workTrackId[WORK_MAX_TRACKS];
    std::string workTrackLabel[WORK_MAX_TRACKS];
    std::string workTrackPrimaryEventTypeId[WORK_MAX_TRACKS];
    float       workTrackToday[WORK_MAX_TRACKS] = {};
    bool        workTrackHasTarget[WORK_MAX_TRACKS] = {};
    float       workTrackTarget[WORK_MAX_TRACKS] = {};
    float       workSeven[WORK_HISTORY_DAYS] = {};
    std::string workNextAction;

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
