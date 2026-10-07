#include "ambient.h"

#include <string>

#include "../net/mazhost.h"
#include "notify.h"
#include "sys.h"

namespace maz {
namespace ambient {
namespace {

bool    gHostOnline = false;
bool    gNeedsMaz = false;
uint8_t gStale = 0;
bool    gInitialised = false;

}  // namespace

void update() {
    // Do not stomp on reminders, recording confirmations or other messages.
    // Leaving the old snapshot untouched means the state change is still
    // noticed as soon as the current toast has gone away.
    if (notify::active()) return;

    if (!gInitialised) {
        gHostOnline = Sys.hostOnline;
        gNeedsMaz = Sys.agentQuestion;
        gStale = Sys.agentsStale;
        gInitialised = true;
        return;
    }

    if (Sys.hostOnline != gHostOnline) {
        gHostOnline = Sys.hostOnline;
        if (Sys.hostOnline)
            notify::post(Note::Success, "hub connected",
                         std::string("COMM ready / ") + host::linkName());
        else
            notify::post(Note::Warn, "hub disconnected",
                         "COMM will retry when needed");
        return;
    }

    if (Sys.agentQuestion && !gNeedsMaz) {
        gNeedsMaz = true;
        notify::post(Note::Warn, "Agent needs you", "open OPS to review");
        return;
    }
    gNeedsMaz = Sys.agentQuestion;

    if (Sys.agentsStale > gStale && !Sys.agentQuestion) {
        gStale = Sys.agentsStale;
        notify::post(Note::Info, "Agent check due",
                     std::to_string(Sys.agentsStale) + " need attention");
        return;
    }
    gStale = Sys.agentsStale;
}

}  // namespace ambient
}  // namespace maz
