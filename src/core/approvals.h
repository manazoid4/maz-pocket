// MAZ Pocket — Claude Code approvals overlay.
//
// A global modal (like the firmware-update confirm): when Core holds a pending
// Claude Code permission request, any screen is covered by tool + summary +
// countdown, and Y/ENTER allow, N/ESC deny, A allow-all for that session.
// Polling is one tiny GET /buddy/summary on the existing host worker, slow
// while idle, a little faster while a request is on screen. See docs/APPROVALS.md.
#pragma once
#include <M5Unified.h>

#include "app.h"

namespace maz {
namespace approvals {

// Poll Core / submit the decision. Returns true when a NEW request just
// arrived (the shell wakes the screen). screenOff stretches the poll interval.
bool update(bool screenOff);
// True while the overlay owns the keyboard.
bool active();
// Consume a key press while active; returns true when swallowed.
bool handleKey(const KeyEvent& e);
void render(M5Canvas& g);

}  // namespace approvals
}  // namespace maz
