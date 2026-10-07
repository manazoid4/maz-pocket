#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "mazhost.h"

namespace maz {
namespace host_worker {

enum class State : uint8_t { Idle, Queued, Running, Done, FailedToStart };
enum class JobKind : uint8_t { None, TalkAudio, PcAction, OutboxAudio, OutboxBeam, BeamPull, SystemStatus, WorkSummary, WorkIncrement, Workflow, Teach, CoreInfo };
enum class WorkflowKind : uint8_t { Plan, Crew, Retro, Prompt };
enum class TeachKind : uint8_t { Displays, Start, Mark, Stop, Status };

struct TalkResult {
    std::string session;
    std::string wavPath;
    std::string speechPath;
    std::string context;
    bool speechReady = false;
    host::Reply reply;
};
struct PcActionResult { std::string action; host::Reply reply; };
struct OutboxAudioResult { std::string recordId; std::string wavPath; std::string context; host::Reply reply; };
struct OutboxBeamResult { std::string recordId; host::Reply reply; };
struct WorkIncrementResult { std::string trackId; host::Reply reply; };
struct WorkflowResult {
    WorkflowKind kind = WorkflowKind::Plan;
    std::string task;
    std::string project;
    std::string templateId;
    host::Reply reply;
};
struct TeachResult {
    TeachKind kind = TeachKind::Status;
    host::TeachStatus status;
    std::vector<host::TeachDisplay> displays;
};

bool submitTalkAudio(const std::string& session, const std::string& wavPath,
                     const std::string& speechPath = "", const std::string& context = "");
bool submitPcAction(const std::string& action, bool retainResult = true);
bool submitOutboxAudio(const std::string& recordId, const std::string& wavPath,
                       const std::string& context = "");
bool submitOutboxBeam(const std::string& recordId, const std::string& text);
bool submitBeamPull();
bool submitCoreInfo();
bool submitSystemStatus();
bool submitWorkSummary();
bool submitWorkIncrement(const std::string& trackId, const std::string& eventTypeId);
bool submitWorkflow(WorkflowKind kind, const std::string& task,
                    const std::string& project = "", const std::string& templateId = "");
bool submitTeach(TeachKind kind, const std::string& sessionId = "",
                 const std::string& displayId = "primary", const std::string& note = "");

bool takeTalkResult(TalkResult& result);
// Reply text as soon as the PC answered, while the voice WAV is still downloading.
bool peekTalkText(std::string& out);
bool takePcActionResult(PcActionResult& result);
bool takeOutboxAudioResult(OutboxAudioResult& result);
bool takeOutboxBeamResult(OutboxBeamResult& result);
bool takeBeamPullResult(host::BeamMessage& result);
bool takeCoreInfoResult(host::CoreInfo& result);
bool takeSystemStatusResult(host::SystemStatus& result);
bool takeWorkSummaryResult(host::WorkSummary& result);
bool takeWorkIncrementResult(WorkIncrementResult& result);
bool takeWorkflowResult(WorkflowResult& result);
bool takeTeachResult(TeachResult& result);

State state();
JobKind jobKind();
bool busy();
const char* stateName();
uint32_t stackHighWaterBytes();

}  // namespace host_worker
}  // namespace maz
