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
std::atomic<JobKind> gKind{JobKind::None};
std::atomic<bool> gRetainPcResult{true};
TaskHandle_t gTask = nullptr;

std::string gSession;
std::string gPath;
std::string gSpeechPath;
std::string gAction;
TalkResult gTalkResult;
PcActionResult gPcResult;
std::atomic<uint32_t> gStackHighWater{0};

void resetToIdle() {
    gSession.clear();
    gPath.clear();
    gSpeechPath.clear();
    gAction.clear();
    gKind.store(JobKind::None, std::memory_order_release);
    gState.store(State::Idle, std::memory_order_release);
}

void finishMeasurement() {
    // ESP-IDF reports this value in bytes. Record it after the expensive
    // HTTP/TLS/TTS path so real ADV soak tests tell us whether 8 KB is
    // appropriately sized instead of guessing from desktop builds.
    const uint32_t highWater =
        static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
    gStackHighWater.store(highWater, std::memory_order_release);
    Serial.printf("[host-worker] stack_free_min=%lu bytes\n",
                  static_cast<unsigned long>(highWater));
}

void worker(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gState.load(std::memory_order_acquire) != State::Queued) continue;

        gState.store(State::Running, std::memory_order_release);
        const JobKind kind = gKind.load(std::memory_order_acquire);

        if (kind == JobKind::TalkAudio) {
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

            gTalkResult.session = std::move(session);
            gTalkResult.wavPath = path;
            gTalkResult.speechPath = speechReady ? speechPath : "";
            gTalkResult.speechReady = speechReady;
            gTalkResult.reply = std::move(reply);
        } else if (kind == JobKind::PcAction) {
            gPcResult.action = gAction;
            gPcResult.reply = host::pcAction(gAction);
        }

        finishMeasurement();

        if (kind == JobKind::PcAction &&
            !gRetainPcResult.load(std::memory_order_acquire)) {
            // Browser actions do not need a result channel. Return the worker
            // to Idle here so a closed tab can never wedge COMM behind Done.
            resetToIdle();
        } else {
            gState.store(State::Done, std::memory_order_release);
        }
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

bool canSubmit() {
    const State current = gState.load(std::memory_order_acquire);
    return current == State::Idle || current == State::FailedToStart;
}

void publish(JobKind kind) {
    gKind.store(kind, std::memory_order_release);
    gState.store(State::Queued, std::memory_order_release);
    xTaskNotifyGive(gTask);
}

}  // namespace

bool submitTalkAudio(const std::string& session, const std::string& wavPath,
                     const std::string& speechPath) {
    if (wavPath.empty() || !canSubmit()) return false;
    if (!ensureWorker()) return false;

    gSession = session;
    gPath = wavPath;
    gSpeechPath = speechPath;
    gRetainPcResult.store(true, std::memory_order_release);
    gTalkResult = TalkResult{};
    gPcResult = PcActionResult{};
    publish(JobKind::TalkAudio);
    return true;
}

bool submitPcAction(const std::string& action, bool retainResult) {
    if (action.empty() || !canSubmit()) return false;
    if (!ensureWorker()) return false;

    gAction = action;
    gRetainPcResult.store(retainResult, std::memory_order_release);
    gTalkResult = TalkResult{};
    gPcResult = PcActionResult{};
    publish(JobKind::PcAction);
    return true;
}

bool takeTalkResult(TalkResult& result) {
    if (gState.load(std::memory_order_acquire) != State::Done ||
        gKind.load(std::memory_order_acquire) != JobKind::TalkAudio)
        return false;

    result = std::move(gTalkResult);
    resetToIdle();
    return true;
}

bool takePcActionResult(PcActionResult& result) {
    if (gState.load(std::memory_order_acquire) != State::Done ||
        gKind.load(std::memory_order_acquire) != JobKind::PcAction)
        return false;

    result = std::move(gPcResult);
    resetToIdle();
    return true;
}

State state() { return gState.load(std::memory_order_acquire); }
JobKind jobKind() { return gKind.load(std::memory_order_acquire); }

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
