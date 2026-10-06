// MAZ Pocket — Nordic UART Service BLE link for the Claude Hardware Buddy API.
//
// Adapted from anthropics/claude-desktop-buddy (src/ble_bridge.*),
// Copyright 2026 Anthropic, PBC, MIT licence. See third_party/claude-desktop-buddy/LICENSE.
//
// Changes from upstream: namespaced, lazy init (BLE stays off until the user
// enables Hardware Buddy, so Wi-Fi-only users keep their heap on a no-PSRAM
// ESP32-S3), and the advertised name carries a MAC suffix.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace maz {
namespace ble_link {

void     begin();            // idempotent; advertises "Claude-MAZ-XXXX"
bool     started();
bool     connected();
bool     secure();           // LE Secure Connections bond complete
uint32_t passkey();          // non-zero while a pairing passkey must be shown
const char* name();
void     clearBonds();
size_t   available();
int      read();
size_t   write(const uint8_t* data, size_t len);

}  // namespace ble_link
}  // namespace maz
