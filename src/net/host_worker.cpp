#include "host_worker.h"

#include <Arduino.h>
#include <atomic>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace maz {
namespace host_worker {
namespace {

constexpr uint32_t WORKER_STACK_BYTES = 9216;
constexpr UBaseType_t WORKER_PRIORITY = 1;

std::atomic<State> gState{State::Idle};
std::atomic<JobKind> gKind{JobKind::None};
std::atomic<bool> gRetainPcResult{true};
TaskHandle_t gTask = nullptr;

std::string gSession, gPath, gSpeechPath, gAction, gContext, gRecordId, gText, gProject, gTemplateId, gDisplayId;
WorkflowKind gWorkflowKind = WorkflowKind::Plan;
TeachKind gTeachKind = TeachKind::Status;

TalkResult gTalkResult;
PcActionResult gPcResult;
OutboxAudioResult gOutboxAudioResult;
OutboxBeamResult gOutboxBeamResult;
host::BeamMessage gBeamPullResult;
host::SystemStatus gSystemResult;
WorkflowResult gWorkflowResult;
TeachResult gTeachResult;
std::atomic<uint32_t> gStackHighWater{0};

void resetToIdle() {
    gSession.clear(); gPath.clear(); gSpeechPath.clear(); gAction.clear(); gContext.clear();
    gRecordId.clear(); gText.clear(); gProject.clear(); gTemplateId.clear(); gDisplayId.clear();
    gKind.store(JobKind::None, std::memory_order_release);
    gState.store(State::Idle, std::memory_order_release);
}

void finishMeasurement() {
    const uint32_t highWater = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
    gStackHighWater.store(highWater, std::memory_order_release);
    Serial.printf("[host-worker] stack_free_min=%lu bytes\n", static_cast<unsigned long>(highWater));
}

host::Reply contextAudio(const std::string& session, const std::string& path,
                         const std::string& context) {
    if (context.empty()) return host::talkAudio(session, path);
    host::Reply transcript = host::transcribe(path);
    if (!transcript.ok) return transcript;
    const std::string words = !transcript.transcript.empty() ? transcript.transcript : transcript.text;
    if (words.empty()) {
        transcript.ok = false;
        transcript.error = "empty transcript";
        return transcript;
    }
    host::Reply reply = host::talkTextContext(session, words, context.substr(0, 700));
    if (reply.ok) reply.transcript = words;
    return reply;
}

host::Reply runWorkflow() {
    switch (gWorkflowKind) {
        case WorkflowKind::Crew: return host::workCrew(gText, gProject);
        case WorkflowKind::Retro: return host::workRetro(gProject, gText);
        case WorkflowKind::Prompt: return host::workPrompt(gTemplateId, gText, gProject);
        default: return host::workPlan(gText, gProject);
    }
}

host::TeachStatus runTeach() {
    switch (gTeachKind) {
        case TeachKind::Start: return host::teachStart(gDisplayId);
        case TeachKind::Mark: return host::teachMark(gSession, gText);
        case TeachKind::Stop: return host::teachStop(gSession);
        default: return host::teachStatus(gSession);
    }
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
            const std::string context = gContext;
            host::Reply reply;
            bool speechReady = false;
            if (session.empty()) session = host::startSession();
            if (session.empty()) reply.error = "PC unreachable";
            else reply = contextAudio(session, path, context);
            if (reply.ok && !speechPath.empty() && !reply.text.empty()) speechReady = host::speak(reply.text, speechPath);
            gTalkResult.session = std::move(session);
            gTalkResult.wavPath = path;
            gTalkResult.speechPath = speechReady ? speechPath : "";
            gTalkResult.context = context;
            gTalkResult.speechReady = speechReady;
            gTalkResult.reply = std::move(reply);
        } else if (kind == JobKind::PcAction) {
            gPcResult.action = gAction;
            gPcResult.reply = host::pcAction(gAction);
        } else if (kind == JobKind::OutboxAudio) {
            std::string session = host::startSession();
            host::Reply reply;
            if (session.empty()) reply.error = "PC unreachable";
            else reply = contextAudio(session, gPath, gContext);
            gOutboxAudioResult.recordId = gRecordId;
            gOutboxAudioResult.wavPath = gPath;
            gOutboxAudioResult.context = gContext;
            gOutboxAudioResult.reply = std::move(reply);
        } else if (kind == JobKind::OutboxBeam) {
            gOutboxBeamResult.recordId = gRecordId;
            gOutboxBeamResult.reply = host::beamSend(gText);
        } else if (kind == JobKind::BeamPull) {
            gBeamPullResult = host::beamPull();
        } else if (kind == JobKind::SystemStatus) {
            gSystemResult = host::systemStatus();
        } else if (kind == JobKind::Workflow) {
            gWorkflowResult.kind = gWorkflowKind;
            gWorkflowResult.task = gText;
            gWorkflowResult.project = gProject;
            gWorkflowResult.templateId = gTemplateId;
            gWorkflowResult.reply = runWorkflow();
        } else if (kind == JobKind::Teach) {
            gTeachResult.kind = gTeachKind;
            if (gTeachKind == TeachKind::Displays) {
                std::string error;
                gTeachResult.displays = host::teachDisplays(error);
                gTeachResult.status.ok = error.empty();
                gTeachResult.status.error = error;
            } else {
                gTeachResult.status = runTeach();
            }
        }

        finishMeasurement();
        if (kind == JobKind::PcAction && !gRetainPcResult.load(std::memory_order_acquire)) resetToIdle();
        else gState.store(State::Done, std::memory_order_release);
    }
}

bool ensureWorker() {
    if (gTask) return true;
    if (xTaskCreate(worker, "maz-host", WORKER_STACK_BYTES, nullptr, WORKER_PRIORITY, &gTask) == pdPASS) return true;
    gState.store(State::FailedToStart, std::memory_order_release);
    return false;
}

bool canSubmit() {
    const State current = gState.load(std::memory_order_acquire);
    return current == State::Idle || current == State::FailedToStart;
}

void clearResults() {
    gTalkResult = TalkResult{}; gPcResult = PcActionResult{}; gOutboxAudioResult = OutboxAudioResult{};
    gOutboxBeamResult = OutboxBeamResult{}; gBeamPullResult = host::BeamMessage{};
    gSystemResult = host::SystemStatus{}; gWorkflowResult = WorkflowResult{}; gTeachResult = TeachResult{};
}

void publish(JobKind kind) {
    gKind.store(kind, std::memory_order_release);
    gState.store(State::Queued, std::memory_order_release);
    xTaskNotifyGive(gTask);
}

}  // namespace

bool submitTalkAudio(const std::string& session, const std::string& wavPath,
                     const std::string& speechPath, const std::string& context) {
    if (wavPath.empty() || !canSubmit() || !ensureWorker()) return false;
    gSession = session; gPath = wavPath; gSpeechPath = speechPath; gContext = context;
    gRetainPcResult.store(true, std::memory_order_release); clearResults(); publish(JobKind::TalkAudio); return true;
}
bool submitPcAction(const std::string& action, bool retainResult) {
    if (action.empty() || !canSubmit() || !ensureWorker()) return false;
    gAction = action; gRetainPcResult.store(retainResult, std::memory_order_release); clearResults(); publish(JobKind::PcAction); return true;
}
bool submitOutboxAudio(const std::string& recordId, const std::string& wavPath,
                       const std::string& context) {
    if (recordId.empty() || wavPath.empty() || !canSubmit() || !ensureWorker()) return false;
    gRecordId = recordId; gPath = wavPath; gContext = context; clearResults(); publish(JobKind::OutboxAudio); return true;
}
bool submitOutboxBeam(const std::string& recordId, const std::string& text) {
    if (recordId.empty() || text.empty() || !canSubmit() || !ensureWorker()) return false;
    gRecordId = recordId; gText = text; clearResults(); publish(JobKind::OutboxBeam); return true;
}
bool submitBeamPull() { if (!canSubmit() || !ensureWorker()) return false; clearResults(); publish(JobKind::BeamPull); return true; }
bool submitSystemStatus() { if (!canSubmit() || !ensureWorker()) return false; clearResults(); publish(JobKind::SystemStatus); return true; }
bool submitWorkflow(WorkflowKind kind, const std::string& task,
                    const std::string& project, const std::string& templateId) {
    if ((task.empty() && kind != WorkflowKind::Retro) || !canSubmit() || !ensureWorker()) return false;
    gWorkflowKind = kind; gText = task; gProject = project; gTemplateId = templateId;
    clearResults(); publish(JobKind::Workflow); return true;
}
bool submitTeach(TeachKind kind, const std::string& sessionId,
                 const std::string& displayId, const std::string& note) {
    if (!canSubmit() || !ensureWorker()) return false;
    if (kind != TeachKind::Start && kind != TeachKind::Displays && sessionId.empty()) return false;
    gTeachKind = kind; gSession = sessionId; gDisplayId = displayId.empty() ? "primary" : displayId; gText = note;
    clearResults(); publish(JobKind::Teach); return true;
}

bool takeTalkResult(TalkResult& result) { if (state()!=State::Done||jobKind()!=JobKind::TalkAudio)return false; result=std::move(gTalkResult); resetToIdle(); return true; }
bool takePcActionResult(PcActionResult& result) { if (state()!=State::Done||jobKind()!=JobKind::PcAction)return false; result=std::move(gPcResult); resetToIdle(); return true; }
bool takeOutboxAudioResult(OutboxAudioResult& result) { if (state()!=State::Done||jobKind()!=JobKind::OutboxAudio)return false; result=std::move(gOutboxAudioResult); resetToIdle(); return true; }
bool takeOutboxBeamResult(OutboxBeamResult& result) { if (state()!=State::Done||jobKind()!=JobKind::OutboxBeam)return false; result=std::move(gOutboxBeamResult); resetToIdle(); return true; }
bool takeBeamPullResult(host::BeamMessage& result) { if (state()!=State::Done||jobKind()!=JobKind::BeamPull)return false; result=std::move(gBeamPullResult); resetToIdle(); return true; }
bool takeSystemStatusResult(host::SystemStatus& result) { if (state()!=State::Done||jobKind()!=JobKind::SystemStatus)return false; result=std::move(gSystemResult); resetToIdle(); return true; }
bool takeWorkflowResult(WorkflowResult& result) { if (state()!=State::Done||jobKind()!=JobKind::Workflow)return false; result=std::move(gWorkflowResult); resetToIdle(); return true; }
bool takeTeachResult(TeachResult& result) { if (state()!=State::Done||jobKind()!=JobKind::Teach)return false; result=std::move(gTeachResult); resetToIdle(); return true; }

State state() { return gState.load(std::memory_order_acquire); }
JobKind jobKind() { return gKind.load(std::memory_order_acquire); }
bool busy() { const State current=state(); return current==State::Queued||current==State::Running; }
const char* stateName() {
    switch (state()) {
        case State::Queued: return "QUEUED";
        case State::Running: return "WORKING";
        case State::Done: return "READY";
        case State::FailedToStart: return "NO MEMORY";
        default: return "IDLE";
    }
}
uint32_t stackHighWaterBytes() { return gStackHighWater.load(std::memory_order_acquire); }

}  // namespace host_worker
}  // namespace maz
