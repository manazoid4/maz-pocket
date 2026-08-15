#pragma once

namespace maz {
namespace ambient {

// Turns meaningful background state changes into short, useful toasts. It does
// no polling/network work of its own, so it cannot slow the UI.
void update();

}  // namespace ambient
}  // namespace maz
