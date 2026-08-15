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

struct CoreProject {
    std::string name;
    std::string branch;
    std::string kind;
    int dirty = 0;
};

struct CoreStatus {
    bool ok = false;
    std::string hostname;
    std::string model;
    int projects = 0;
    bool ollama = false;
    std::string error;
};

struct CoreJob {
    bool ok = false;
    std::string id;
    std::string state;
    std::string action;
    std::string project;
    std::string output;
    std::string error;
};

bool configured();
bool health();
const char* linkName();
std::string startSession();
Reply talkText(const std::string& session, const std::string& text);
Reply talkAudio(const std::string& session, const std::string& wavPath);
Reply transcribe(const std::string& wavPath);
Reply brainDump(const std::string& wavPath, const std::vector<uint32_t>& highlights);
Reply pcAction(const std::string& action);
bool speak(const std::string& text, const std::string& wavPath);
Assurance assurance();
Reply sendNudge(const std::string& sessionId);

CoreStatus coreStatus();
std::vector<CoreProject> coreProjects(std::string& error);
Reply coreAction(const std::string& action, const std::string& project);
CoreJob coreStartJob(const std::string& action, const std::string& project);
CoreJob coreJob(const std::string& id);

}  // namespace host
}  // namespace maz
