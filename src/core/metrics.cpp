#include "metrics.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../net/comm_stream.h"
#include "../net/host_async.h"

namespace maz {
namespace metrics {

Snapshot snapshot() {
    Snapshot out;
    out.uptimeMs = millis();
    out.freeHeap = ESP.getFreeHeap();
    out.minFreeHeap = esp_get_minimum_free_heap_size();
    out.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    out.mainStackMinWords = uxTaskGetStackHighWaterMark(nullptr);

    const auto host = host_async::stats();
    out.hostSubmitted = host.submitted;
    out.hostCompleted = host.completed;
    out.hostRejected = host.rejected;
    out.hostCancelled = host.cancelled;
    out.hostResultDrops = host.resultDrops;
    out.hostLastLatencyMs = host.lastLatencyMs;
    out.hostMaxLatencyMs = host.maxLatencyMs;
    out.hostStackMinWords = host.stackMinWords;
    out.hostQueueDepth = host.requestDepth;
    out.hostQueueHighWater = host.requestHighWater;

    const auto ws = comm_stream::stats();
    out.wsTurns = ws.turns;
    out.wsFramesQueued = ws.framesQueued;
    out.wsFramesSent = ws.framesSent;
    out.wsFrameDrops = ws.frameDrops;
    out.wsReconnects = ws.reconnects;
    out.wsProtocolErrors = ws.protocolErrors;
    out.wsFirstTokenMs = ws.lastFirstTokenMs;
    out.wsTotalMs = ws.lastTotalMs;
    out.wsStackMinWords = ws.stackMinWords;
    out.wsQueueDepth = ws.queueDepth;
    out.wsQueueHighWater = ws.queueHighWater;
    return out;
}

}  // namespace metrics
}  // namespace maz
