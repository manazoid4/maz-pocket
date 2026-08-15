#include "apps.h"

#include <cstring>

namespace maz {
namespace apps {

namespace {
// v0.03.1 makes the six product surfaces visible on Home. Existing utilities
// stay registered and searchable through Ctrl+K/shortcuts without becoming
// separate top-level products.
const Descriptor TABLE[] = {
    {"home",      "Home",        nullptr,   "home menu",                                      0,     false, makeHome},
    {"talk",      "Call PC",     "COMM",    "call pc voice communicator remote control",      KEY_T, true,  makeCallV3},
    {"braindump", "Field Log",   "CAPTURE", "capture thought field log highlight voice",       KEY_B, true,  makeCapture},
    {"nudge",     "Agent Ops",   "OPS",     "agent fleet operations nudge assurance sync",    KEY_N, true,  makeAgentsV3},
    {"tools",     "Desk",        "DESK",    "pc desk controls diagnostics tools system",       0,     true,  makeTools},
    {"inbox",     "Recall",      "RECALL",  "answers results useful outputs recall recent",    KEY_I, true,  makeInbox},
    {"reminders", "Flow",        "FLOW",    "reminder schedule done snooze workflow focus",    KEY_R, true,  makeReminders},

    // Showcase extras stay out of Home by design.
    {"snake",     "Snake",       nullptr, "snake game arcade retro fun",               0, false, makeSnake},
    {"hyperdrive","Hyperdrive",  nullptr, "hyperdrive starfield imu tilt motion demo", 0, false, makeHyperdrive},

    {"focus",     "Focus",       nullptr, "focus timer pomodoro session",            KEY_F, false, makeFocus},
    {"sprint",    "Sprint",      nullptr, "sprint outcome timer debrief",            KEY_S, false, makeSprint},
    {"decision",  "Decision",    nullptr, "decision what why reason",                KEY_D, false, makeDecision},
    {"notes",     "Notes",       nullptr, "notes note write text",                   0,     false, makeNotes},
    {"tasks",     "Tasks",       nullptr, "tasks todo next",                         0,     false, makeTasks},
    {"recorder",  "Recorder",    nullptr, "recorder record memo audio wav",          0,     false, makeRecorder},
    {"calc",      "Calculator",  "Calc",  "calculator calc math sum",                KEY_C, false, makeCalculator},
    {"stopwatch", "Stopwatch",   nullptr, "stopwatch timer lap count",               KEY_W, false, makeStopwatch},
    {"qr",        "Beam",        "Beam",  "beam qr code share phone url text wifi",   KEY_Q, false, makeQr},
    {"viewer",    "Text Viewer", "Viewer","viewer read file text md txt",            KEY_V, false, makeViewer},
    {"gen",       "Generator",   nullptr, "generator password passphrase random",    KEY_G, false, makeGenerator},
    {"snippets",  "Snippets",    nullptr, "snippets text clip email phrase",         KEY_P, false, makeSnippets},
    {"wifi",      "Connections", "Wi-Fi", "wifi network connections host maz",       0,     false, makeConnections},
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
