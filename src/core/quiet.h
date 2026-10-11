// nod - Quiet mode: mute the chirps without missing what matters.
//
// Device-side only. Three levels, with an optional auto-expiry so it cannot be
// forgotten. The sound POLICY below is pure (no hardware, no globals) so it can
// be unit tested on a PC; the state and the clock live in quiet.cpp.
//
// Toasts are never suppressed at any level: a needs/error toast always shows.
#pragma once
#include <stdint.h>

namespace maz {
namespace quiet {

enum class Level : uint8_t { Normal = 0, Quiet = 1, Silent = 2 };
enum class Ev : uint8_t { None, Done, Usage, Needs, Error };  // None = not an agent event

// May this event make a sound? (Cfg.uiSounds is still applied afterwards, in sfx.)
//   Normal: yes.  Quiet: done/usage silent, needs/error and non-agent sounds as usual.
//   Silent: never.
inline bool shouldChirp(Level lv, Ev ev) {
    if (lv == Level::Silent) return false;
    if (lv == Level::Quiet) return ev != Ev::Done && ev != Ev::Usage;
    return true;
}

// Wrap-safe millis() compare (valid for spans under ~24 days).
inline bool expired(uint32_t now, uint32_t until) {
    return static_cast<int32_t>(now - until) >= 0;
}

// Duration choices offered to the owner. Index 3 = until turned off.
constexpr uint8_t DUR_COUNT = 4;
constexpr uint32_t DUR_MS[DUR_COUNT] = {30u * 60000u, 2u * 3600000u, 8u * 3600000u, 0u};
inline const char* durLabel(uint8_t i) {
    static const char* const L[DUR_COUNT] = {"30 min", "2 h", "8 h", "until off"};
    return L[i < DUR_COUNT ? i : 1];
}

Level       level();                       // current level; Normal after every boot
void        set(Level lv, uint8_t dur);    // dur = index into DUR_MS; Normal ignores it
void        cycle();                       // Normal -> Quiet -> Silent -> Normal (Fn+Q)
void        update();                      // call every loop: handles the expiry
int         remainingMin();                // minutes left, -1 if not timed
const char* statusText();                  // "QUIET" / "SILENT", nullptr when Normal
const char* levelName(Level lv);

}  // namespace quiet
}  // namespace maz
