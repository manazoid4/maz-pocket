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
std::string startSession();
Reply talkText(const std::string& session, const std::string& text);
Reply talkAudio(const std::string& session, const std::string& wavPath);
// Speech to text with no model round-trip and no session: this backs typing by
// voice, where the only correct answer is exactly what was said.
Reply transcribe(const std::string& wavPath);
Reply brainDump(const std::string& wavPath, const std::vector<uint32_t>& highlights);
Assurance assurance();
Reply sendNudge(const std::string& sessionId);

}  // namespace host
}  // namespace maz
