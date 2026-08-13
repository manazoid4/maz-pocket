// MAZ Pocket — shared drawing primitives.
// Apps never talk to M5.Display directly; they draw into the shell's canvas
// through these helpers so status bar, hints and dialogs stay consistent.
#pragma once
#include <M5Unified.h>

#include <string>

#include "theme.h"

namespace maz {
namespace ui {

// --- chrome ---------------------------------------------------------------
void statusBar(M5Canvas& g);                   // clock, wifi, rec, battery
void hintBar(M5Canvas& g, const char* hints);  // e.g. "ENTER open   ESC back"
void header(M5Canvas& g, const char* title, const char* right = nullptr);

// --- widgets --------------------------------------------------------------
void panel(M5Canvas& g, int x, int y, int w, int h, uint16_t fill = theme::PANEL);
void listRow(M5Canvas& g, int visibleIndex, bool selected, const char* label,
             const char* right = nullptr);
void scrollBar(M5Canvas& g, int total, int firstVisible, int visible);
void emptyState(M5Canvas& g, const char* line1, const char* line2 = nullptr);
void bigValue(M5Canvas& g, const char* value, const char* caption,
              uint16_t colour = theme::TEXT);
void progress(M5Canvas& g, int x, int y, int w, int h, float pct, uint16_t colour);

// --- MAZ identity ---------------------------------------------------------
// The mark: a struck ring with a solid core. Drawn, never bitmapped, so it
// stays crisp at any size and carries no third-party asset licence.
void mark(M5Canvas& g, int cx, int cy, int r, uint16_t colour, float energy = 0.f);
void wordmark(M5Canvas& g, int cx, int y, uint16_t colour);

// --- text -----------------------------------------------------------------
std::string ellipsis(const std::string& s, size_t maxChars);
std::string hhmmss(uint32_t seconds);
std::string humanSize(size_t bytes);
std::string stamp(uint32_t epoch);  // "2026-08-13 14:05"

}  // namespace ui
}  // namespace maz
