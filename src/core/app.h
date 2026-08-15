// MAZ Pocket — the app contract.
#pragma once
#include <M5Unified.h>

#include <string>

#include "keys.h"

namespace maz {

class App {
public:
    virtual ~App() = default;

    virtual const char* id() const    = 0;
    virtual const char* title() const = 0;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual bool onKey(const KeyEvent&) { return false; }

    virtual void        update() {}
    virtual void        render(M5Canvas&) = 0;
    virtual const char* hints() const { return "ESC back"; }

    // Tiny, bounded context for Fn+Space Context Ask. Apps should describe only
    // the currently visible/selected item — never dump a database or secrets.
    virtual std::string contextSnapshot() const { return title(); }

    void invalidate() { _dirty = true; }
    bool dirty() const { return _dirty; }
    void clean() { _dirty = false; }

private:
    bool _dirty = true;
};

}  // namespace maz
