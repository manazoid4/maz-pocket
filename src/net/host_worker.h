#pragma once

#include <stdint.h>
#include <string>

#include "mazhost.h"

namespace maz {
namespace host_worker {

enum class State : uint8_t { Idle, Queued, Running, Done, FailedToStart };

struct TalkResult {
    std::string session;
    std::string wavPath;
    std::string speechPath;
    bool speechReady = false;
    host::Reply reply;
};

// One bounded background Host job is deliberate. The Cardputer ADV is a small
// control surface, not a server; serialising expensive Host work prevents a
// burst of UI actions from creating several HTTP/TLS stacks at once.
// If speechPath is non-empty, successful replies are also rendered to WAV by
// MAZ Host on the worker task so TTS cannot stall the UI task either.
bool submitTalkAudio(const std::string& session, const std::string& wavPath,
                     const std::string& speechPath = "");

// Takes ownership of the one completed result and returns the worker to Idle.
// The result contains copied data only; no App pointer crosses task boundaries.
bool takeTalkResult(TalkResult& result);

State state();
bool busy();
const char* stateName();
uint32_t stackHighWaterBytes();

}  // namespace host_worker
}  // namespace maz
