// MAZ Pocket — one notification path for the whole device.
//
// Focus finishing, a recording saved, Wi-Fi dropping and (next version) an
// assistant reply all arrive through this same call, so the future AI features
// inherit a notification UI that already exists and already feels right.
#pragma once
#include <M5Unified.h>

#include <string>

namespace maz {

enum class Note : uint8_t { Info, Success, Warn, Error };

namespace notify {

void post(Note kind, const std::string& title, const std::string& detail = "");
void update();             // ages out the current toast
void render(M5Canvas& g);  // drawn on top of whatever app is running
bool active();
void dismiss();

}  // namespace notify
}  // namespace maz
