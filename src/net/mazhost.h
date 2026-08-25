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

// WORK Consistency glance payload. Server-side hard cap (spec): at most 4
// pinned tracks, a fixed 7-length day array — mirrored here as fixed-size,
// bounded fields so a malformed/oversized response cannot overflow anything.
constexpr int WORK_MAX_TRACKS = 4;
constexpr int WORK_HISTORY_DAYS = 7;

struct WorkTrack {
    std::string id;
    std::string shortLabel;
    float todayTotal = 0;
    bool hasTarget = false;
    float target = 0;
};

struct WorkSummary {
    bool ok = false;
    int trackCount = 0;
    WorkTrack tracks[WORK_MAX_TRACKS];
    float sevenDay[WORK_HISTORY_DAYS] = {};
    std::string error;
};

struct TeachDisplay {
    std::string id;
    std::string name;
    int width = 0;
    int height = 0;
    bool primary = false;
};

struct TeachStatus {
    bool ok = false;
    std::string sessionId;
    std::string state;
    std::string transcript;
    std::string error;
    int frameCount = 0;
    int elapsedSeconds = 0;
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

// Explicit PC screen demonstration capture. Recording is performed by MAZ Core
// and is never a hidden background recorder; the Pocket starts, marks and stops
// a named session.
std::vector<TeachDisplay> teachDisplays(std::string& error);
TeachStatus teachStart(const std::string& displayId);
TeachStatus teachMark(const std::string& sessionId, const std::string& note = "");
TeachStatus teachStop(const std::string& sessionId);
TeachStatus teachStatus(const std::string& sessionId);

// v0.7 FIELD endpoints. Received Beam content is data only and is never used as
// a command. Telemetry is a cached one-shot laptop snapshot, not a stream.
Reply beamSend(const std::string& text);
BeamMessage beamPull();
SystemStatus systemStatus();
WorkSummary workSummary();

CoreStatus coreStatus();
std::vector<CoreProject> coreProjects(std::string& error);
Reply coreAction(const std::string& action, const std::string& project);
CoreJob coreStartJob(const std::string& action, const std::string& project);
CoreJob coreJob(const std::string& id);

}  // namespace host
}  // namespace maz
