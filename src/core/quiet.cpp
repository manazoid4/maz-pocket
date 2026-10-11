#include "quiet.h"

#include <Arduino.h>

#include "notify.h"
#include "settings.h"

namespace maz {
namespace quiet {

namespace {
// Boot state is always Normal and the timer is RAM-only: a reboot can never
// leave the device muted. Cfg.quietLevel/quietDur only remember the last choice.
Level    gLevel = Level::Normal;
uint32_t gUntil = 0;
bool     gTimed = false;
}  // namespace

Level level() { return gLevel; }

const char* levelName(Level lv) {
    return lv == Level::Silent ? "Silent" : (lv == Level::Quiet ? "Quiet" : "Normal");
}

const char* statusText() {
    return gLevel == Level::Silent ? "SILENT" : (gLevel == Level::Quiet ? "QUIET" : nullptr);
}

int remainingMin() {
    if (gLevel == Level::Normal || !gTimed) return -1;
    const int32_t left = static_cast<int32_t>(gUntil - millis());
    return left <= 0 ? 0 : static_cast<int>((left + 59999) / 60000);
}

void set(Level lv, uint8_t dur) {
    if (dur >= DUR_COUNT) dur = 1;
    gLevel = lv;
    gTimed = lv != Level::Normal && DUR_MS[dur] != 0;
    gUntil = millis() + DUR_MS[dur];
    Cfg.quietLevel = static_cast<uint8_t>(lv);
    Cfg.quietDur   = dur;
}

void cycle() {
    const Level next = gLevel == Level::Normal ? Level::Quiet
                     : gLevel == Level::Quiet  ? Level::Silent
                                               : Level::Normal;
    set(next, Cfg.quietDur);
    Cfg.save();
    notify::post(Note::Info, std::string("Sound: ") + levelName(next),
                 next == Level::Normal ? "chirps on" : durLabel(Cfg.quietDur));
}

void update() {
    if (gLevel == Level::Normal || !gTimed || !expired(millis(), gUntil)) return;
    gLevel = Level::Normal;
    gTimed = false;
    notify::post(Note::Info, "Sound back to normal", "quiet time is over");
}

}  // namespace quiet
}  // namespace maz
