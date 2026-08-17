#include "apps.h"

#include <cstring>

namespace maz {
namespace apps {

namespace {
const Descriptor TABLE[] = {
    {"home",      "Home",        nullptr,   "home menu now quick actions field",                    0,     false, makeHome},
    {"talk",      "Call MAZ",    "CALL",   "call maz voice assistant conversation playback cloud local context ask", KEY_T, true,  makeComm},
    {"braindump", "Capture",     "CAPTURE","capture thought field log highlight voice",            KEY_B, true,  makeCapture},
    {"agents",    "Agents",      "AGENTS", "agents plan crew retro nudge status assurance sync",   KEY_N, true,  makeAgentsHub},
    {"desk",      "Control",     "CONTROL","control wifi pc device storage settings core laptop debug pairing phone", 0, true, makeDesk},
    {"recall",    "Memory",      "MEMORY", "memory inbox notes prompt deck snippets viewer skills knowledge", 0, true, makeRecall},
    {"flow",      "Focus",       "FOCUS",  "focus reminders sprint tasks shift workflows",         0,     true,  makeFlow},

    {"nudge",     "Agent Status", "STATUS", "agent status nudge assurance sync evidence",          0, false, makeAgentsV3},
    {"plan",      "Plan",        "PLAN",   "plan think before code project implementation risk authority", 0, false, makePlan},
    {"prompts",   "Prompt Deck", "PROMPTS","prompt deck templates build bug review research maz", 0, false, makePromptDeck},
    {"crew",      "Crew",        "CREW",   "crew agents split work claude codex hermes collision", 0, false, makeCrew},
    {"retro",     "Retro",       "RETRO",  "retro learn session improve template skill knowledge guard", 0, false, makeRetro},

    {"control",   "Control Center", "CTRL", "control center device network pc core screen",   0,     false, makeControlCenter},
    {"network",   "Wi-Fi",       "WI-FI",  "wifi network scan connect reconnect hotspot",     KEY_W, false, makeNetworkV5},
    {"core",      "Projects & Builds", "CORE", "core pc projects builds tests git local ai",  0,     false, makeCoreConsole},
    {"pairing",   "Pairing & Phone", "PAIR", "pairing token token id phone approval full control", 0, false, makePairing},
    {"laptop",    "Laptop Status", "LAP",  "laptop cpu ram gpu vram battery ollama status",    0,     false, makeLaptop},
    {"beam",      "Send to PC",  "BEAM",   "beam laptop pocket text url clipboard offline",    0,     false, makeBeam},
    {"shift",     "Shift Clock", "SHIFT",  "shift work field elapsed timer log",               0,     false, makeShift},

    {"snake",     "Snake",       nullptr, "snake game arcade retro fun",               0, false, makeSnake},
    {"hyperdrive","Hyperdrive",  nullptr, "hyperdrive starfield imu tilt motion demo", 0, false, makeHyperdrive},

    {"inbox",     "Results Inbox", nullptr, "answers results useful outputs",          KEY_I, false, makeInbox},
    {"focus",     "Focus Timer", nullptr, "focus timer pomodoro session",              KEY_F, false, makeFocus},
    {"reminders", "Reminders",   nullptr, "reminder schedule done snooze",             KEY_R, false, makeReminders},
    {"sprint",    "Work Sprint", nullptr, "sprint outcome timer debrief",              KEY_S, false, makeSprint},
    {"decision",  "Decision",    nullptr, "decision what why reason",                  KEY_D, false, makeDecision},
    {"notes",     "Notes",       nullptr, "notes note write text",                     0,     false, makeNotes},
    {"tasks",     "Tasks",       nullptr, "tasks todo next",                           0,     false, makeTasks},
    {"recorder",  "Voice Recorder", nullptr, "recorder record memo audio wav",         0,     false, makeRecorder},
    {"calc",      "Calculator",  "Calc",  "calculator calc math sum",                  KEY_C, false, makeCalculator},
    {"stopwatch", "Stopwatch",   nullptr, "stopwatch timer lap count",                 0,     false, makeStopwatch},
    {"qr",        "QR",          "QR",    "qr code share phone url text wifi",         KEY_Q, false, makeQr},
    {"viewer",    "Text Viewer", "Viewer","viewer read file text md txt",              KEY_V, false, makeViewer},
    {"gen",       "Generator",   nullptr, "generator password passphrase random",      KEY_G, false, makeGenerator},
    {"snippets",  "Snippets",    nullptr, "snippets text clip email phrase",           KEY_P, false, makeSnippets},
    {"wifi",      "Legacy Connections", "Conn", "legacy wifi host setup",              0,     false, makeConnections},
    {"tools",     "Device Tests", nullptr, "tools system diagnostics test",             0,     false, makeTools},
    {"settings",  "Settings",    nullptr, "settings brightness volume config",         0,     false, makeSettings},
    {"help",      "Keys & Help", "Help",  "help keys shortcuts about version",         KEY_H, false, makeHelp},
};
}  // namespace

const Descriptor* table(size_t& count) {
    count = sizeof(TABLE) / sizeof(TABLE[0]);
    return TABLE;
}

const Descriptor* find(const char* id) {
    for (const auto& d : TABLE)
        if (!strcmp(d.id, id)) return &d;
    return nullptr;
}

App* create(const char* id) {
    const Descriptor* d = find(id);
    return d ? d->make() : nullptr;
}

}  // namespace apps
}  // namespace maz
