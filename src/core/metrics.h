#pragma once

#include <stdint.h>

namespace maz {
namespace metrics {

struct Snapshot {
    uint32_t uptimeMs = 0;
    uint32_t freeHeap = 0;
    uint32_t minFreeHeap = 0;
    uint32_t largestFreeBlock = 0;
    uint32_t mainStackMinWords = 0;

    uint32_t hostSubmitted = 0;
    uint32_t hostCompleted = 0;
    uint32_t hostRejected = 0;
    uint32_t hostCancelled = 0;
    uint32_t hostResultDrops = 0;
    uint32_t hostLastLatencyMs = 0;
    uint32_t hostMaxLatencyMs = 0;
    uint32_t hostStackMinWords = 0;
    uint8_t hostQueueDepth = 0;
    uint8_t hostQueueHighWater = 0;

    uint32_t wsTurns = 0;
    uint32_t wsFramesQueued = 0;
    uint32_t wsFramesSent = 0;
    uint32_t wsFrameDrops = 0;
    uint32_t wsReconnects = 0;
    uint32_t wsProtocolErrors = 0;
    uint32_t wsFirstTokenMs = 0;
    uint32_t wsTotalMs = 0;
    uint32_t wsStackMinWords = 0;
    uint8_t wsQueueDepth = 0;
    uint8_t wsQueueHighWater = 0;
};

Snapshot snapshot();

}  // namespace metrics
}  // namespace maz
