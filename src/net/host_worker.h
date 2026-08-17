#pragma once

#include <stdint.h>
#include <string>

#include "mazhost.h"

namespace maz {
namespace host_worker {

enum class State : uint8_t { Idle, Queued, Running, Done, FailedToStart };
enum class JobKind : uint8_t {
    None,
    TalkAudio,
    PcAction,
    OutboxAudio,
    OutboxBeam,
    BeamPull,
    SystemStatus,
    Workflow,
};

enum class WorkflowKind : uint8_t { Plan, Crew, Retro, Prompt };

struct TalkResult {
    std::string session;
    std::string wavPath;
    std::string speechPath;
    std::string context;
    bool speechReady = false;
    host::Reply reply;
};

struct PcActionResult {
    std::string action;
    host::Reply reply;
};

struct OutboxAudioResult {
    std::string recordId;
    std::string wavPath;
    std::string context;
    host::Reply reply;
};

struct OutboxBeamResult {
    std::string recordId;
    host::Reply reply;
};

struct WorkflowResult {
    WorkflowKind kind = WorkflowKind::Plan;
    std::string task;
    std::string project;
    std::string templateId;
    host::Reply reply;
};

bool submitTalkAudio(const std::string& session, const std::string& wavPath,
                     const std::string& speechPath = "",
                     const std::string& context = "");
bool submitPcAction(const std::string& action, bool retainResult = true);
bool submitOutboxAudio(const std::string& recordId, const std::string& wavPath,
                       const std::string& context = "");
bool submitOutboxBeam(const std::string& recordId, const std::string& text);
bool submitBeamPull();
bool submitSystemStatus();
bool submitWorkflow(WorkflowKind kind, const std::string& task,
                    const std::string& project = "",
                    const std::string& templateId = "");

bool takeTalkResult(TalkResult& result);
bool takePcActionResult(PcActionResult& result);
bool takeOutboxAudioResult(OutboxAudioResult& result);
bool takeOutboxBeamResult(OutboxBeamResult& result);
bool takeBeamPullResult(host::BeamMessage& result);
bool takeSystemStatusResult(host::SystemStatus& result);
bool takeWorkflowResult(WorkflowResult& result);

State state();
JobKind jobKind();
bool busy();
const char* stateName();
uint32_t stackHighWaterBytes();

}  // namespace host_worker
}  // namespace maz
