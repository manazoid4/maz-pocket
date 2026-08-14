#pragma once

#include <string>
#include <vector>

namespace maz {
namespace host {

struct Reply {
    bool        ok = false;
    int         status = 0;
    std::string text;
    std::string transcript;
    std::string provider;
    std::string error;
    std::string reminderTitle;
    uint32_t    reminderDelay = 0;
};

struct Agent {
    std::string id;
    std::string name;
    std::string provider;
    std::string state;
    std::string sessionState;
    std::string evidence;
};

struct Assurance {
    bool ok = false;
    std::string state = "OFFLINE";
    int working = 0;
    int waiting = 0;
    int needsNudge = 0;
    int overdue = 0;
    int attention = 0;
    int questionForMaz = 0;
    std::vector<Agent> agents;
    std::string error;
};

bool configured();
bool health();
const char* linkName();  // LAN / REMOTE / OFFLINE
std::string startSession();
Reply talkText(const std::string& session, const std::string& text);
Reply talkAudio(const std::string& session, const std::string& wavPath);
Reply transcribe(const std::string& wavPath);
Reply brainDump(const std::string& wavPath, const std::vector<uint32_t>& highlights);
Reply pcAction(const std::string& action);
// Ask MAZ Host to synthesize a short reply and save the WAV directly into the
// existing storage abstraction. No cloud TTS key ever reaches the Cardputer.
bool speak(const std::string& text, const std::string& wavPath);
Assurance assurance();
Reply sendNudge(const std::string& sessionId);

}  // namespace host
}  // namespace maz
