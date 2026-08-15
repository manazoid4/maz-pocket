// MAZ Pocket — the app registry.
#pragma once
#include <stddef.h>

#include "../core/app.h"

namespace maz {
namespace store { struct Record; }
namespace apps {

struct Descriptor {
    const char* id;
    const char* title;
    const char* shortTitle;
    const char* keywords;
    uint8_t     shortcut;
    bool        onHome;
    App* (*make)();

    const char* cellTitle() const { return shortTitle ? shortTitle : title; }
};

const Descriptor* table(size_t& count);
const Descriptor* find(const char* id);
App*              create(const char* id);

App* makeHome();
App* makeCall();
App* makeCallV3();
App* makeCapture();
App* makeDesk();
App* makeRecall();
App* makeFlow();
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
App* makeAgentsV3();
App* makeReminders();
App* makeSnake();
App* makeHyperdrive();
void scheduleReminder(store::Record& reminder, uint32_t delaySeconds);
void updateProductServices();

}  // namespace apps
}  // namespace maz
