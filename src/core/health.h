#pragma once

#include <stdint.h>

namespace maz {
namespace health {

// Lightweight runtime instrumentation for the Cardputer ADV. This never
// reboots the device or allocates large buffers; it records evidence so real
// hardware limits can be tuned from measurements rather than guesses.
void begin();
void observeLoop(uint32_t elapsedMs);
void update();

}  // namespace health
}  // namespace maz
