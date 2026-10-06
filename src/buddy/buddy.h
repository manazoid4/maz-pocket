// MAZ Pocket — Claude Hardware Buddy protocol.
//
// Speaks the Claude desktop app's Hardware Buddy wire protocol
// (newline-delimited JSON over BLE NUS; see docs/HARDWARE-BUDDY.md) so the
// Cardputer can watch Claude Code / Cowork sessions and approve or deny tool
// calls from the keyboard. Works alongside MAZ Core: Core is Wi-Fi, Buddy is
// BLE straight to the Claude desktop app, no API key or Core needed.
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

namespace maz {
namespace buddy {

struct State {
    uint8_t  total = 0, running = 0, waiting = 0;
    uint32_t tokensToday = 0;
    std::string msg;
    std::vector<std::string> entries;  // newest first, capped
    std::string promptId, promptTool, promptHint;
    std::string owner;
    uint32_t approved = 0, denied = 0;
};

void boot();            // starts BLE only if the user enabled Buddy before
void update();          // call every loop(); cheap when disabled
void enable();          // turn on now and persist
bool enabled();

bool linked();          // heartbeat seen in the last 30 s
bool secure();
bool hasPrompt();
uint32_t passkey();
const char* deviceName();
const State& state();

bool approve();         // "once"; false if no prompt / not linked
bool deny();

}  // namespace buddy
}  // namespace maz
