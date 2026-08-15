#pragma once

namespace maz {
namespace web {

// Device-hosted admin surface. It only binds once Wi-Fi is up; write actions
// require the existing MAZ pairing token, which never appears in the page.
void begin();
void update();
bool running();

}  // namespace web
}  // namespace maz
