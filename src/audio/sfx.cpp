#include "sfx.h"

#include <M5Unified.h>

#include "../core/settings.h"

namespace maz {
namespace sfx {

namespace {
// `force` exists so an alarm the user asked for (Focus complete) still fires
// with UI sounds muted — muting chirps is not the same as muting alarms.
void beep(float hz, uint32_t ms, bool force = false) {
    if (!Cfg.uiSounds && !force) return;
    M5.Speaker.tone(hz, ms);
}
void pair(float a, float b, uint32_t ms, bool force = false) {
    beep(a, ms, force);
    delay(ms);
    beep(b, ms, force);
}
}  // namespace

void boot()     { pair(660.f, 990.f, 70); }
void select()   { beep(1200.f, 8); }
void confirm()  { beep(1600.f, 18); }
void recStart() { pair(880.f, 1320.f, 40); }
void recStop()  { pair(1320.f, 880.f, 40); }
void saved()    { beep(1760.f, 45); }
void error()    { pair(300.f, 200.f, 90); }

void timerDone() {
    for (int i = 0; i < 3; ++i) {
        beep(1200.f, 120, /*force=*/true);
        delay(160);
    }
}

void forNote(Note kind) {
    switch (kind) {
        case Note::Success: saved(); break;
        case Note::Warn:    beep(700.f, 60); break;
        case Note::Error:   error(); break;
        default:            beep(1000.f, 20); break;
    }
}

}  // namespace sfx
}  // namespace maz
