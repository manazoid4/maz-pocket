#include "apps.h"

#include <cstring>

namespace maz {
namespace apps {

namespace {
const Descriptor TABLE[] = {
    {"home",      "Home",        nullptr,   "home menu",                                      0,     false, makeHome},
    {"talk",      "Call PC",     "COMM",   "call pc voice communicator remote control",      KEY_T, true,  makeComm},
    {"braindump", "Capture",     "CAPTURE","capture thought field log highlight voice",      KEY_B, true,  makeCaptureV51},
    {"nudge",     "Agent Ops",   "OPS",    "agent fleet operations nudge assurance sync",    KEY_N, true,  makeOps},
    {"desk",      "Control",     "CONTROL","control wifi pc device storage settings core",   0,     true,  makeDesk},
    {"recall",    "Recall",      "RECALL", "recall inbox notes snippets viewer outputs",     0,     true,  makeRecall},
    {"flow",      "Flow",        "FLOW",   "flow reminders focus sprint tasks workflows",    0,     true,  makeFlow},

    {"control",   "Control Center", "CTRL", "control center device network pc core screen",   0,     false, makeControlCenter},
    {"network",   "Wi-Fi",       "WI-FI",  "wifi network scan connect reconnect hotspot",     KEY_W, false, makeNetworkV5},
    {"core",      "MAZ Core",    "CORE",   "core pc projects builds tests git local ai",      0,     false, makeCoreConsole},
    {"runtime",   "Runtime",     "RUNTIME","runtime metrics heap stack queues latency websocket",0,false, makeRuntimeMetrics},

    {"snake",     "Snake",       nullptr, "snake game arcade retro fun",               0, false, makeSnake},
    {"hyperdrive","Hyperdrive",  nullptr, "hyperdrive starfield imu tilt motion demo", 0, false, makeHyperdrive},

    {"inbox",     "Inbox",       nullptr, "answers results useful outputs",          KEY_I, false, makeInbox},
    {"focus",     "Focus",       nullptr, "focus timer pomodoro session",            KEY_F, false, makeFocus},
    {"reminders", "Reminders",   nullptr, "reminder schedule done snooze",           KEY_R, false, makeReminders},
    {"sprint",    "Sprint",      nullptr, "sprint outcome timer debrief",            KEY_S, false, makeSprint},
    {"decision",  "Decision",    nullptr, "decision what why reason",                KEY_D, false, makeDecision},
    {"notes",     "Notes",       nullptr, "notes note write text",                   0,     false, makeNotes},
    {"tasks",     "Tasks",       nullptr, "tasks todo next",                         0,     false, makeTasks},
    {"recorder",  "Recorder",    nullptr, "recorder record memo audio wav",          0,     false, makeRecorder},
    {"calc",      "Calculator",  "Calc",  "calculator calc math sum",                KEY_C, false, makeCalculator},
    {"stopwatch", "Stopwatch",   nullptr, "stopwatch timer lap count",               0,     false, makeStopwatch},
    {"qr",        "Beam",        "Beam",  "beam qr code share phone url text wifi",   KEY_Q, false, makeQr},
    {"viewer",    "Text Viewer", "Viewer","viewer read file text md txt",            KEY_V, false, makeViewer},
    {"gen",       "Generator",   nullptr, "generator password passphrase random",    KEY_G, false, makeGenerator},
    {"snippets",  "Snippets",    nullptr, "snippets text clip email phrase",         KEY_P, false, makeSnippets},
    {"wifi",      "Legacy Connections", "Conn", "legacy wifi host setup",            0,     false, makeConnections},
    {"tools",     "Tools",       nullptr, "tools system diagnostics test",           0,     false, makeTools},
    {"settings",  "Settings",    nullptr, "settings brightness volume config",       0,     false, makeSettings},
    {"help",      "Keys & Help", "Help",  "help keys shortcuts about version",       KEY_H, false, makeHelp},
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
