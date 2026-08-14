#pragma once

namespace maz::launcher {

// Invalidates only this Launcher-managed OTA app, then reboots into whatever
// the bootloader falls through to — M5Launcher's TEST partition when it is
// installed, otherwise a factory image or another populated slot.
//
// Returns false, having changed nothing, when there is no bootable partition
// to land in. Flashing MAZ Pocket directly over USB replaces M5Launcher, and
// in that state invalidating this image would leave the device reachable only
// by cable.
bool reboot();

}  // namespace maz::launcher
