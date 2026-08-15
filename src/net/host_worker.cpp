#include "host_worker.h"

#include <Arduino.h>
#include <atomic>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace maz {
namespace host_worker {
namespace {

constexpr uint32_t WORKER_STACK_BYTES = 8192;
constexpr UBaseType_t WORKER_PRIORITY = 1;

std::atomic<State> gState{State::Idle};
TaskHandle_t gTask = nullptr;
std::string gSession;
std::string gPath;
std::string gSpeechPath;
TalkResult gResult;
std::atomic<uint32_t> gStackHighWater{0};

void worker(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gState.load(std::memory_order_acquire) != State::Queued) continue;

        gState.store(State::Running, std::memory_order_release);

        // submitTalkAudio writes these before publishing Queued. They remain
        // immutable until this job publishes Done.
        std::string session = gSession;
        const std::string path = gPath;
        const std::string speechPath = gSpeechPath;
        host::Reply reply;
        bool speechReady = false;

        if (session.empty()) session = host::startSession();
        if (session.empty()) {
            reply.error = "PC unreachable";
        } else {
            reply = host::talkAudio(session, path);
        }

        if (reply.ok && !speechPath.empty() && !reply.text.empty())
            speechReady = host::speak(reply.text, speechPath);

        gResult.session = std::move(session);
        gResult.wavPath = path;
        gResult.speechPath = speechReady ? speechPath : "";
        gResult.speechReady = speechReady;
        gResult.reply = std::move(reply);

        // ESP-IDF reports this value in bytes. Record it after the expensive
        // HTTP/TLS/TTS path so real ADV soak tests tell us whether 8 KB is
        // appropriately sized instead of guessing from desktop builds.
        const uint32_t highWater =
            static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
        gStackHighWater.store(highWater, std::memory_order_release);
        Serial.printf("[host-worker] stack_free_min=%lu bytes\n",
                      static_cast<unsigned long>(highWater));
        gState.store(State::Done, std::memory_order_release);
    }
}

bool ensureWorker() {
    if (gTask) return true;
    if (xTaskCreate(worker, "maz-host", WORKER_STACK_BYTES, nullptr,
                    WORKER_PRIORITY, &gTask) == pdPASS)
        return true;
    gState.store(State::FailedToStart, std::memory_order_release);
    return false;
}

}  // namespace

bool submitTalkAudio(const std::string& session, const std::string& wavPath,
                     const std::string& speechPath) {
    if (wavPath.empty()) return false;
    const State current = gState.load(std::memory_order_acquire);
    if (current != State::Idle && current != State::FailedToStart) return false;
    if (!ensureWorker()) return false;

    gSession = session;
    gPath = wavPath;
    gSpeechPath = speechPath;
    gResult = TalkResult{};

    gState.store(State::Queued, std::memory_order_release);
    xTaskNotifyGive(gTask);
    return true;
}

bool takeTalkResult(TalkResult& result) {
    if (gState.load(std::memory_order_acquire) != State::Done) return false;

    result = std::move(gResult);
    gSession.clear();
    gPath.clear();
    gSpeechPath.clear();
    gState.store(State::Idle, std::memory_order_release);
    return true;
}

State state() { return gState.load(std::memory_order_acquire); }

bool busy() {
    const State current = state();
    return current == State::Queued || current == State::Running;
}

const char* stateName() {
    switch (state()) {
        case State::Queued: return "QUEUED";
        case State::Running: return "WORKING";
        case State::Done: return "READY";
        case State::FailedToStart: return "NO MEMORY";
        default: return "IDLE";
    }
}

uint32_t stackHighWaterBytes() {
    return gStackHighWater.load(std::memory_order_acquire);
}

}  // namespace host_worker
}  // namespace maz
