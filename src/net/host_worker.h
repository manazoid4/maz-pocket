#pragma once

#include <stdint.h>
#include <string>

#include "mazhost.h"

namespace maz {
namespace host_worker {

enum class State : uint8_t { Idle, Queued, Running, Done, FailedToStart };
enum class JobKind : uint8_t { None, TalkAudio, PcAction };

struct TalkResult {
    std::string session;
    std::string wavPath;
    std::string speechPath;
    bool speechReady = false;
    host::Reply reply;
};

struct PcActionResult {
    std::string action;
    host::Reply reply;
};

// One bounded background Host job is deliberate. The Cardputer ADV is a small
// control surface, not a server; serialising expensive Host work prevents a
// burst of UI/web actions from creating several HTTP/TLS stacks at once.
bool submitTalkAudio(const std::string& session, const std::string& wavPath,
                     const std::string& speechPath = "");

// retainResult=true is used by COMM because it needs to render the result.
// Web controls use false: the action is allow-listed and fire-and-forget, so a
// browser disappearing cannot leave the single Host worker permanently Done.
bool submitPcAction(const std::string& action, bool retainResult = true);

// Takes ownership of the matching completed result and returns the worker to
// Idle. Results contain copied data only; no App pointer crosses task bounds.
bool takeTalkResult(TalkResult& result);
bool takePcActionResult(PcActionResult& result);

State state();
JobKind jobKind();
bool busy();
const char* stateName();
uint32_t stackHighWaterBytes();

}  // namespace host_worker
}  // namespace maz
