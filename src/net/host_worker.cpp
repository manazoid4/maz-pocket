#include "host_worker.h"

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
std::string gResultSession;
std::string gResultPath;
host::Reply gResult;
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
        host::Reply reply;

        if (session.empty()) session = host::startSession();
        if (session.empty()) {
            reply.error = "PC unreachable";
        } else {
            reply = host::talkAudio(session, path);
        }

        gResultSession = std::move(session);
        gResultPath = path;
        gResult = std::move(reply);
        // ESP-IDF reports this value in bytes.
        gStackHighWater.store(
            static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)),
            std::memory_order_release);
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

bool submitTalkAudio(const std::string& session, const std::string& wavPath) {
    if (wavPath.empty()) return false;
    if (gState.load(std::memory_order_acquire) != State::Idle) return false;
    if (!ensureWorker()) return false;

    gSession = session;
    gPath = wavPath;
    gResultSession.clear();
    gResultPath.clear();
    gResult = host::Reply{};

    gState.store(State::Queued, std::memory_order_release);
    xTaskNotifyGive(gTask);
    return true;
}

bool takeTalkResult(std::string& session, std::string& wavPath, host::Reply& reply) {
    if (gState.load(std::memory_order_acquire) != State::Done) return false;

    session = std::move(gResultSession);
    wavPath = std::move(gResultPath);
    reply = std::move(gResult);
    gSession.clear();
    gPath.clear();
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
