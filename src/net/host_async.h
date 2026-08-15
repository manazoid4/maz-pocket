#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "mazhost.h"

namespace maz {
namespace host_async {

enum class Op : uint8_t {
    None,
    TalkAudio,
    BrainDump,
    PcAction,
    Assurance,
    Nudge,
    Speak,
    CoreStatus,
    CoreProjects,
    CoreStartJob,
    CoreJob,
};

struct Result {
    uint32_t id = 0;
    Op op = Op::None;
    bool cancelled = false;
    bool boolValue = false;
    uint32_t latencyMs = 0;
    std::string session;
    std::string error;
    host::Reply reply;
    host::Assurance assurance;
    host::CoreStatus coreStatus;
    std::vector<host::CoreProject> projects;
    host::CoreJob coreJob;
};

struct Stats {
    uint32_t submitted = 0;
    uint32_t completed = 0;
    uint32_t rejected = 0;
    uint32_t cancelled = 0;
    uint32_t resultDrops = 0;
    uint32_t lastLatencyMs = 0;
    uint32_t maxLatencyMs = 0;
    uint32_t stackMinWords = 0;
    uint8_t requestDepth = 0;
    uint8_t requestHighWater = 0;
};

bool begin();
void update();

uint32_t talkAudio(const std::string& session, const std::string& wavPath);
uint32_t brainDump(const std::string& wavPath,
                   const std::vector<uint32_t>& highlights);
uint32_t pcAction(const std::string& action);
uint32_t assurance();
uint32_t sendNudge(const std::string& sessionId);
uint32_t speak(const std::string& text, const std::string& wavPath);
uint32_t coreStatus();
uint32_t coreProjects();
uint32_t coreStartJob(const std::string& action, const std::string& project);
uint32_t coreJob(const std::string& id);

// Best-effort cancellation. It never blocks the UI waiting for an in-flight
// HTTP socket; it suppresses the result and lets the worker finish/clean up.
void cancel(uint32_t id);

// Results are retained in a small bounded main-task inbox until consumed.
bool poll(uint32_t id, Result& out);
Stats stats();

}  // namespace host_async
}  // namespace maz
