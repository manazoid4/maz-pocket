#include "health.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sys.h"

namespace maz {
namespace health {
namespace {

uint32_t gLastSampleMs = 0;
uint32_t gLoopMaxMs = 0;

constexpr uint32_t SAMPLE_MS = 30000;
constexpr uint32_t HEAP_WARN_BYTES = 48 * 1024;
constexpr uint32_t LARGEST_WARN_BYTES = 24 * 1024;
constexpr uint32_t LOOP_WARN_MS = 250;

void sample(bool force) {
    const uint32_t now = millis();
    if (!force && now - gLastSampleMs < SAMPLE_MS) return;
    gLastSampleMs = now;

    Sys.freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Sys.minFreeHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    Sys.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    // ESP-IDF's FreeRTOS port reports the high-water mark in bytes (unlike
    // vanilla FreeRTOS), so do not multiply by sizeof(StackType_t).
    Sys.mainStackHighWater = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
    Sys.loopMaxMs = gLoopMaxMs;
    gLoopMaxMs = 0;

    Sys.memoryPressure = Sys.freeHeap < HEAP_WARN_BYTES ||
                         Sys.largestFreeBlock < LARGEST_WARN_BYTES;
    Sys.uiStallObserved = Sys.loopMaxMs >= LOOP_WARN_MS;

    Serial.printf("[health] heap=%lu min=%lu largest=%lu stack=%lu loop_max=%lums pressure=%s stall=%s\n",
                  static_cast<unsigned long>(Sys.freeHeap),
                  static_cast<unsigned long>(Sys.minFreeHeap),
                  static_cast<unsigned long>(Sys.largestFreeBlock),
                  static_cast<unsigned long>(Sys.mainStackHighWater),
                  static_cast<unsigned long>(Sys.loopMaxMs),
                  Sys.memoryPressure ? "yes" : "no",
                  Sys.uiStallObserved ? "yes" : "no");
}

}  // namespace

void begin() { sample(true); }

void observeLoop(uint32_t elapsedMs) {
    if (elapsedMs > gLoopMaxMs) gLoopMaxMs = elapsedMs;
}

void update() { sample(false); }

}  // namespace health
}  // namespace maz
