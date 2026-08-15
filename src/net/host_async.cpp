#include "host_async.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <algorithm>
#include <vector>

namespace maz {
namespace host_async {
namespace {

constexpr uint8_t REQUEST_CAPACITY = 4;
constexpr uint8_t RESULT_CAPACITY = 8;
constexpr uint8_t DONE_CAPACITY = 8;
constexpr uint8_t CANCEL_CAPACITY = 8;

struct Request {
    uint32_t id = 0;
    Op op = Op::None;
    std::string a;
    std::string b;
    std::vector<uint32_t> marks;
};

QueueHandle_t gRequests = nullptr;
QueueHandle_t gResults = nullptr;
TaskHandle_t gTask = nullptr;
std::vector<Result*> gDone;
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t gCancelled[CANCEL_CAPACITY] = {};
uint8_t gCancelPos = 0;
volatile uint32_t gNextId = 1;
volatile uint32_t gSubmitted = 0;
volatile uint32_t gCompleted = 0;
volatile uint32_t gRejected = 0;
volatile uint32_t gCancelledCount = 0;
volatile uint32_t gResultDrops = 0;
volatile uint32_t gLastLatency = 0;
volatile uint32_t gMaxLatency = 0;
volatile uint32_t gStackMinWords = 0;
volatile uint8_t gRequestHighWater = 0;

uint32_t nextId() {
    portENTER_CRITICAL(&gMux);
    uint32_t id = gNextId++;
    if (gNextId == 0) gNextId = 1;
    portEXIT_CRITICAL(&gMux);
    return id;
}

void rememberCancelled(uint32_t id) {
    if (!id) return;
    portENTER_CRITICAL(&gMux);
    gCancelled[gCancelPos++ % CANCEL_CAPACITY] = id;
    portEXIT_CRITICAL(&gMux);
}

bool isCancelled(uint32_t id) {
    if (!id) return false;
    bool found = false;
    portENTER_CRITICAL(&gMux);
    for (uint8_t i = 0; i < CANCEL_CAPACITY; ++i) {
        if (gCancelled[i] == id) {
            found = true;
            break;
        }
    }
    portEXIT_CRITICAL(&gMux);
    return found;
}

uint32_t submit(Op op, const std::string& a = {}, const std::string& b = {},
                const std::vector<uint32_t>& marks = {}) {
    if (!gRequests) return 0;
    Request* request = new Request();
    if (!request) return 0;
    request->id = nextId();
    request->op = op;
    request->a = a;
    request->b = b;
    request->marks = marks;
    if (xQueueSend(gRequests, &request, 0) != pdTRUE) {
        delete request;
        ++gRejected;
        return 0;
    }
    ++gSubmitted;
    const auto depth = static_cast<uint8_t>(uxQueueMessagesWaiting(gRequests));
    if (depth > gRequestHighWater) gRequestHighWater = depth;
    return request->id;
}

void publish(Result* result) {
    if (!result || !gResults) {
        delete result;
        return;
    }
    if (xQueueSend(gResults, &result, 0) == pdTRUE) return;

    // Prefer the newest completion. A full result queue means the UI has not
    // drained it for several loops; discard the oldest bounded item rather
    // than ever blocking the network worker or allocating an unbounded list.
    Result* old = nullptr;
    if (xQueueReceive(gResults, &old, 0) == pdTRUE) delete old;
    ++gResultDrops;
    if (xQueueSend(gResults, &result, 0) != pdTRUE) {
        delete result;
        ++gResultDrops;
    }
}

void worker(void*) {
    for (;;) {
        Request* request = nullptr;
        if (xQueueReceive(gRequests, &request, portMAX_DELAY) != pdTRUE || !request)
            continue;

        Result* result = new Result();
        if (!result) {
            delete request;
            continue;
        }
        result->id = request->id;
        result->op = request->op;
        const uint32_t started = millis();

        if (isCancelled(request->id)) {
            result->cancelled = true;
            result->error = "cancelled";
        } else {
            switch (request->op) {
                case Op::TalkAudio: {
                    result->session = request->a;
                    if (result->session.empty()) result->session = host::startSession();
                    if (result->session.empty()) {
                        result->reply.error = "session unavailable";
                        result->error = result->reply.error;
                    } else {
                        result->reply = host::talkAudio(result->session, request->b);
                        result->error = result->reply.error;
                    }
                    break;
                }
                case Op::BrainDump:
                    result->reply = host::brainDump(request->a, request->marks);
                    result->error = result->reply.error;
                    break;
                case Op::PcAction:
                    result->reply = host::pcAction(request->a);
                    result->error = result->reply.error;
                    break;
                case Op::Assurance:
                    result->assurance = host::assurance();
                    result->error = result->assurance.error;
                    break;
                case Op::Nudge:
                    result->reply = host::sendNudge(request->a);
                    result->error = result->reply.error;
                    break;
                case Op::Speak:
                    result->boolValue = host::speak(request->a, request->b);
                    if (!result->boolValue) result->error = "speech unavailable";
                    break;
                case Op::CoreStatus:
                    result->coreStatus = host::coreStatus();
                    result->error = result->coreStatus.error;
                    break;
                case Op::CoreProjects: {
                    std::string error;
                    result->projects = host::coreProjects(error);
                    result->error = error;
                    break;
                }
                case Op::CoreStartJob:
                    result->coreJob = host::coreStartJob(request->a, request->b);
                    result->error = result->coreJob.error;
                    break;
                case Op::CoreJob:
                    result->coreJob = host::coreJob(request->a);
                    result->error = result->coreJob.error;
                    break;
                default:
                    result->error = "unsupported async operation";
                    break;
            }
        }

        result->latencyMs = millis() - started;
        if (isCancelled(request->id)) {
            result->cancelled = true;
            result->error = "cancelled";
        }
        gLastLatency = result->latencyMs;
        if (result->latencyMs > gMaxLatency) gMaxLatency = result->latencyMs;
        const uint32_t stack = uxTaskGetStackHighWaterMark(nullptr);
        if (!gStackMinWords || stack < gStackMinWords) gStackMinWords = stack;
        ++gCompleted;
        publish(result);
        delete request;
    }
}

}  // namespace

bool begin() {
    if (gTask) return true;
    gRequests = xQueueCreate(REQUEST_CAPACITY, sizeof(Request*));
    gResults = xQueueCreate(RESULT_CAPACITY, sizeof(Result*));
    if (!gRequests || !gResults) return false;
    if (xTaskCreate(worker, "maz-host", 6144, nullptr, 1, &gTask) != pdPASS) {
        gTask = nullptr;
        return false;
    }
    gDone.reserve(DONE_CAPACITY);
    return true;
}

void update() {
    if (!gResults) return;
    Result* result = nullptr;
    while (xQueueReceive(gResults, &result, 0) == pdTRUE) {
        if (!result) continue;
        if (gDone.size() >= DONE_CAPACITY) {
            delete gDone.front();
            gDone.erase(gDone.begin());
            ++gResultDrops;
        }
        gDone.push_back(result);
    }
}

uint32_t talkAudio(const std::string& session, const std::string& wavPath) {
    return submit(Op::TalkAudio, session, wavPath);
}
uint32_t brainDump(const std::string& wavPath,
                   const std::vector<uint32_t>& highlights) {
    return submit(Op::BrainDump, wavPath, {}, highlights);
}
uint32_t pcAction(const std::string& action) { return submit(Op::PcAction, action); }
uint32_t assurance() { return submit(Op::Assurance); }
uint32_t sendNudge(const std::string& sessionId) { return submit(Op::Nudge, sessionId); }
uint32_t speak(const std::string& text, const std::string& wavPath) {
    return submit(Op::Speak, text, wavPath);
}
uint32_t coreStatus() { return submit(Op::CoreStatus); }
uint32_t coreProjects() { return submit(Op::CoreProjects); }
uint32_t coreStartJob(const std::string& action, const std::string& project) {
    return submit(Op::CoreStartJob, action, project);
}
uint32_t coreJob(const std::string& id) { return submit(Op::CoreJob, id); }

void cancel(uint32_t id) {
    if (!id) return;
    rememberCancelled(id);
    ++gCancelledCount;
}

bool poll(uint32_t id, Result& out) {
    if (!id) return false;
    update();
    for (auto it = gDone.begin(); it != gDone.end(); ++it) {
        Result* result = *it;
        if (!result || result->id != id) continue;
        out = *result;
        delete result;
        gDone.erase(it);
        return true;
    }
    return false;
}

Stats stats() {
    Stats out;
    out.submitted = gSubmitted;
    out.completed = gCompleted;
    out.rejected = gRejected;
    out.cancelled = gCancelledCount;
    out.resultDrops = gResultDrops;
    out.lastLatencyMs = gLastLatency;
    out.maxLatencyMs = gMaxLatency;
    out.stackMinWords = gStackMinWords;
    out.requestDepth = gRequests ? static_cast<uint8_t>(uxQueueMessagesWaiting(gRequests)) : 0;
    out.requestHighWater = gRequestHighWater;
    return out;
}

}  // namespace host_async
}  // namespace maz
