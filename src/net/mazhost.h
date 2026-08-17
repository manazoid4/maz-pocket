#pragma once

#include <stdint.h>
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

struct BeamMessage {
    bool ok = false;
    bool hasMessage = false;
    std::string id;
    std::string kind;
    std::string text;
    std::string error;
};

struct SystemStatus {
    bool ok = false;
    int cpuPct = -1;
    int ramPct = -1;
    int batteryPct = -1;
    bool charging = false;
    bool gpuAvailable = false;
    int gpuPct = -1;
    int vramUsedMb = 0;
    int vramTotalMb = 0;
    int gpuTempC = -1;
    bool ollamaOnline = false;
    bool ollamaLoaded = false;
    std::string ollamaModel;
    int ollamaVramMb = 0;
    int ollamaContext = 0;
    std::string error;
};

bool configured();
bool health();
const char* linkName();
std::string startSession();
Reply talkText(const std::string& session, const std::string& text);
Reply talkTextContext(const std::string& session, const std::string& text,
                      const std::string& context);
Reply talkAudio(const std::string& session, const std::string& wavPath);
Reply transcribe(const std::string& wavPath);
Reply brainDump(const std::string& wavPath, const std::vector<uint32_t>& highlights);
Reply pcAction(const std::string& action);
bool speak(const std::string& text, const std::string& wavPath);
Assurance assurance();
Reply sendNudge(const std::string& sessionId);

// v0.8 agent workbench. These are compact request/response calls; the Pocket
// displays the useful summary while MAZ Core keeps richer workflow data.
Reply workPlan(const std::string& task, const std::string& project = "");
Reply workCrew(const std::string& task, const std::string& project = "");
Reply workRetro(const std::string& project = "", const std::string& note = "");
Reply workPrompt(const std::string& templateId, const std::string& task,
                 const std::string& project = "");

// v0.7 FIELD endpoints. Received Beam content is data only and is never used as
// a command. Telemetry is a cached one-shot laptop snapshot, not a stream.
Reply beamSend(const std::string& text);
BeamMessage beamPull();
SystemStatus systemStatus();

CoreStatus coreStatus();
std::vector<CoreProject> coreProjects(std::string& error);
Reply coreAction(const std::string& action, const std::string& project);
CoreJob coreStartJob(const std::string& action, const std::string& project);
CoreJob coreJob(const std::string& id);

}  // namespace host
}  // namespace maz
