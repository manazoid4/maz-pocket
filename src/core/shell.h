// MAZ Pocket — the shell.
//
// Owns the frame: one canvas, one app stack, the status/hint chrome, the
// global shortcuts, the command palette, the screen timeout and the Focus
// timer (which must keep counting while you are somewhere else entirely).
#pragma once
#include <M5Unified.h>

#include <string>

#include "app.h"

namespace maz {
namespace shell {

bool begin();
void loop();

// Navigation. push() takes ownership.
void push(App* app);
bool pushById(const char* id);
void pop();
void goHome();
int  depth();

M5Canvas& canvas();
void      invalidate();  // force a repaint of the current app
void      wake();        // any input resets the dim/sleep countdown

void openPalette();

// The Focus timer lives here rather than in the Focus app, because a timer
// that dies when you leave the screen is not a timer.
namespace focus {
void               start(uint32_t seconds, const std::string& label);
void               pause();
void               resume();
void               cancel();
bool               running();
bool               paused();
uint32_t           remaining();
uint32_t           total();
const std::string& label();
}  // namespace focus

}  // namespace shell
}  // namespace maz
