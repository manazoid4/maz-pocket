#pragma once

namespace maz::launcher {

// Invalidates only this Launcher-managed OTA app, then reboots into the
// official M5Launcher TEST partition. Returns only if the hand-back fails.
bool reboot();

}  // namespace maz::launcher
