// MAZ Pocket — the app contract.
//
// Every screen is an App with the same lifecycle, which is what makes the
// future assistant modules (Call-over-network, Recall, Forge, Pulse) additive
// rather than surgical: they implement this interface and register a factory.
#pragma once
#include <M5Unified.h>

#include "keys.h"

namespace maz {

class App {
public:
    virtual ~App() = default;

    virtual const char* id() const    = 0;  // stable, used by command palette
    virtual const char* title() const = 0;

    virtual void onEnter() {}
    virtual void onExit() {}

    // Return true if the key was consumed. Unconsumed keys fall through to the
    // shell, which owns ESC-to-back and the global shortcuts.
    virtual bool onKey(const KeyEvent&) { return false; }

    virtual void        update() {}  // logic tick, ~60Hz
    virtual void        render(M5Canvas&) = 0;
    virtual const char* hints() const { return "ESC back"; }

    // Apps mark themselves dirty; the shell only pushes pixels when something
    // changed, which is what keeps idle battery drain sane.
    void invalidate() { _dirty = true; }
    bool dirty() const { return _dirty; }
    void clean() { _dirty = false; }

private:
    bool _dirty = true;
};

}  // namespace maz
