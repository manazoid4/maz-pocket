#include "apps.h"

#include <cstring>

namespace maz {
namespace apps {

namespace {
// Order matters twice: Home pages through this table (the `onHome` entries
// first, then the rest, eight to a page), and the palette uses this order to
// break ties between equally scored matches.
//
// Every app now carries a letter shortcut or a page position, so there is no
// longer such a thing as an app you can only reach by already knowing its
// name. Letters are given where they are genuinely mnemonic; the rest open by
// their 1-8 digit on the page they appear. C/W/Q/V/G/P/H were all unbound.
//
//   id          title          shortTitle  keywords                                shortcut  onHome  factory
const Descriptor TABLE[] = {
    {"home",      "Home",        nullptr,   "home menu",                            0,     false, makeHome},
    {"talk",      "Talk",        nullptr,   "maz talk ask voice agent assistant",    KEY_T, true,  makeCall},
    {"braindump", "BrainDump",   nullptr,   "capture thought highlight voice",       KEY_B, true,  makeCapture},
    {"inbox",     "Inbox",       nullptr,   "answers results useful outputs",        KEY_I, true,  makeInbox},
    {"decision",  "Decision",    nullptr,   "decision what why reason",              KEY_D, true,  makeDecision},
    {"focus",     "Focus",       nullptr,   "focus timer pomodoro session",          KEY_F, true,  makeFocus},
    {"sprint",    "Sprint",      nullptr,   "sprint outcome timer debrief",          KEY_S, true,  makeSprint},
    {"nudge",     "Nudge",       nullptr,   "agent fleet assurance sync waiting",    KEY_N, true,  makeNudge},
    {"reminders", "Reminders",   nullptr,   "reminder schedule done snooze",         KEY_R, true,  makeReminders},
    {"notes",     "Notes",       nullptr,   "notes note write text",                 0,     false, makeNotes},
    {"tasks",     "Tasks",       nullptr,   "tasks todo next",                       0,     false, makeTasks},
    {"recorder",  "Recorder",    nullptr,   "recorder record memo audio wav",        0,     false, makeRecorder},
    {"calc",      "Calculator",  "Calc",    "calculator calc math sum",              KEY_C, false, makeCalculator},
    {"stopwatch", "Stopwatch",   nullptr,   "stopwatch timer lap count",             KEY_W, false, makeStopwatch},
    {"qr",        "QR Code",     "QR",      "qr code share url wifi",                KEY_Q, false, makeQr},
    {"viewer",    "Text Viewer", "Viewer",  "viewer read file text md txt",          KEY_V, false, makeViewer},
    {"gen",       "Generator",   nullptr,   "generator password passphrase random",  KEY_G, false, makeGenerator},
    {"snippets",  "Snippets",    nullptr,   "snippets text clip email phrase",       KEY_P, false, makeSnippets},
    {"wifi",      "Connections", "Wi-Fi",   "wifi network connections host maz",     0,     false, makeConnections},
    {"tools",     "Tools",       nullptr,   "tools system diagnostics test",         0,     false, makeTools},
    {"settings",  "Settings",    nullptr,   "settings brightness volume config",     0,     false, makeSettings},
    {"help",      "Keys & Help", "Help",    "help keys shortcuts about version",     KEY_H, false, makeHelp},
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
