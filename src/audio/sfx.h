// MAZ Pocket — UI sounds.
//
// Short, quiet, two notes at most. The point is confirmation you can feel
// without looking at the screen: you hold SPACE in your pocket, hear the
// capture chirp, talk, hear the save chirp. All of it respects Cfg.uiSounds.
#pragma once
#include "../core/notify.h"

namespace maz {
namespace sfx {

void boot();       // device ready
void select();     // menu move
void confirm();    // enter / open
void recStart();   // microphone live
void recStop();    // microphone released
void saved();      // written to storage
void timerDone();  // Focus complete — the one sound allowed to be insistent
void error();

void forNote(Note kind);

}  // namespace sfx
}  // namespace maz
