#pragma once

#include <stdint.h>
#include <string>

#include "mazhost.h"

namespace maz {
namespace host_worker {

enum class State : uint8_t { Idle, Queued, Running, Done, FailedToStart };

// One bounded background Host job is deliberate. The Cardputer ADV is a small
// control surface, not a server; serialising expensive Host work prevents a
// burst of UI actions from creating several HTTP/TLS stacks at once.
bool submitTalkAudio(const std::string& session, const std::string& wavPath);

// Takes ownership of the one completed result and returns the worker to Idle.
// The result contains copied data only; no App pointer crosses task boundaries.
bool takeTalkResult(std::string& session, std::string& wavPath, host::Reply& reply);

State state();
bool busy();
const char* stateName();
uint32_t stackHighWaterBytes();

}  // namespace host_worker
}  // namespace maz
