#include "apps.h"

#include <cstring>

namespace maz {
namespace apps {

namespace {
// Order matters twice: Home draws the `onHome` entries as its grid, and the
// palette uses this order to break ties between equally scored matches.
const Descriptor TABLE[] = {
    {"home",      "Home",        "home menu",                         0,     false, makeHome},
    {"talk",      "Talk",        "maz talk ask voice agent assistant", KEY_T, true, makeCall},
    {"braindump", "BrainDump",   "capture thought highlight voice",    KEY_B, true, makeCapture},
    {"inbox",     "Inbox",       "answers results useful outputs",     KEY_I, true, makeInbox},
    {"decision",  "Decision",    "decision what why reason",           KEY_D, true, makeDecision},
    {"focus",     "Focus",       "focus timer pomodoro session",       KEY_F, true, makeFocus},
    {"sprint",    "Sprint",      "sprint outcome timer debrief",       KEY_S, true, makeSprint},
    {"nudge",     "Nudge",       "agent fleet assurance sync waiting", KEY_N, true, makeNudge},
    {"reminders", "Reminders",   "reminder schedule done snooze",      KEY_R, true, makeReminders},
    {"notes",     "Notes",       "notes note write text",              0, false, makeNotes},
    {"tasks",     "Tasks",       "tasks todo next",                    0, false, makeTasks},
    {"recorder",  "Recorder",    "recorder record memo audio wav",     0, false, makeRecorder},
    {"calc",      "Calculator",  "calculator calc math sum",          0,     false, makeCalculator},
    {"stopwatch", "Stopwatch",   "stopwatch timer lap count",         0,     false, makeStopwatch},
    {"qr",        "QR Code",     "qr code share url wifi",            0,     false, makeQr},
    {"viewer",    "Text Viewer", "viewer read file text md txt",      0,     false, makeViewer},
    {"gen",       "Generator",   "generator password passphrase random", 0,  false, makeGenerator},
    {"snippets",  "Snippets",    "snippets text clip email phrase",   0,     false, makeSnippets},
    {"wifi",      "Connections", "wifi network connections host maz", 0,     false, makeConnections},
    {"tools",     "Tools",       "tools system diagnostics test",     0,     false, makeTools},
    {"settings",  "Settings",    "settings brightness volume config", 0,     false, makeSettings},
    {"help",      "Keys & Help", "help keys shortcuts about version", 0,     false, makeHelp},
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
