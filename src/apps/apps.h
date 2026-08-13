// MAZ Pocket — the app registry.
//
// One table drives Home's grid, the command palette and the global shortcuts,
// so adding an assistant module later means adding one row here, not editing
// three screens. `keywords` is what the palette actually matches against.
#pragma once
#include <stddef.h>

#include "../core/app.h"

namespace maz {
namespace store { struct Record; }
namespace apps {

struct Descriptor {
    const char* id;
    const char* title;
    const char* keywords;  // space separated, lower case
    uint8_t     shortcut;  // HID code that opens it from Home, 0 = none
    bool        onHome;    // shown in the Home grid
    App* (*make)();
};

const Descriptor* table(size_t& count);
const Descriptor* find(const char* id);
App*              create(const char* id);

// Factories. Defined across apps/*.cpp; declared here so the registry is the
// only file that needs to know they all exist.
App* makeHome();
App* makeCall();
App* makeCapture();
App* makeNotes();
App* makeFocus();
App* makeTasks();
App* makeRecorder();
App* makeTools();
App* makeSettings();
App* makeConnections();
App* makeSnippets();
App* makeCalculator();
App* makeStopwatch();
App* makeQr();
App* makeViewer();
App* makeGenerator();
App* makeHelp();
App* makeInbox();
App* makeDecision();
App* makeSprint();
App* makeNudge();
App* makeReminders();
void scheduleReminder(store::Record& reminder, uint32_t delaySeconds);
void updateProductServices();

}  // namespace apps
}  // namespace maz
