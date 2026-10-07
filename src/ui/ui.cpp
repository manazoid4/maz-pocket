#include "ui.h"

#include <time.h>

#include "../core/settings.h"
#include "../core/sys.h"
#include "../net/mazhost.h"

namespace maz {
namespace ui {

using namespace theme;

// ---------------------------------------------------------------- identity
void mark(M5Canvas& g, int cx, int cy, int r, uint16_t colour, float energy) {
    // Ring + core. `energy` (0..1) swells a second ring: that is the listening
    // animation, reused by Call, Capture and Recorder so "MAZ is hearing you"
    // always looks the same wherever you are.
    if (energy > 0.f) {
        const int er = r + 3 + static_cast<int>(energy * 7.f);
        g.drawCircle(cx, cy, er, ACCENT2);
        if (energy > 0.55f) g.drawCircle(cx, cy, er + 4, PANEL);
    }
    g.drawCircle(cx, cy, r, colour);
    g.drawCircle(cx, cy, r - 1, colour);
    g.fillCircle(cx, cy, r / 2, colour);
    // The strike through the ring's lower right: the one detail that makes
    // this mark MAZ and not a generic record dot.
    g.drawLine(cx + r - 1, cy + r - 1, cx + r + 3, cy + r + 3, colour);
}

void wordmark(M5Canvas& g, int cx, int y, uint16_t colour) {
    g.setTextDatum(top_center);
    g.setTextColor(colour, BG);
    g.setFont(&fonts::Font4);
    g.drawString("MAZ", cx, y);
    g.setFont(&fonts::Font2);
    g.setTextColor(DIM, BG);
    g.drawString("POCKET", cx, y + 26);
    g.setTextDatum(top_left);
}

// ------------------------------------------------------------------ chrome
static void batteryGlyph(M5Canvas& g, int x, int y, int pct, bool charging) {
    const uint16_t c =
        charging ? ACCENT2 : (pct <= 15 ? ERR : (pct <= 30 ? WARN : DIM));
    g.drawRect(x, y, 18, 9, c);
    g.fillRect(x + 18, y + 3, 2, 3, c);
    if (pct > 0) g.fillRect(x + 2, y + 2, (14 * pct) / 100, 5, c);
}

void statusBar(M5Canvas& g) {
    g.fillRect(0, 0, SCREEN_W, STATUS_H, PANEL);
    g.drawFastHLine(0, STATUS_H - 1, SCREEN_W, LINE);
    g.setFont(&fonts::Font0);
    g.setTextDatum(top_left);

    // Left: the mark, small, always present — the device's signature.
    g.fillCircle(6, 7, 3, ACCENT);
    g.drawCircle(6, 7, 5, ACCENT);

    g.setTextColor(TEXT, PANEL);
    if (Sys.timeValid) {
        time_t    t = time(nullptr);
        struct tm tmv;
        localtime_r(&t, &tmv);
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        g.drawString(buf, 16, 4);
    } else {
        g.setTextColor(DIM, PANEL);
        g.drawString("--:--", 16, 4);
    }

    int x = 48;
    // Live activity beats everything else for attention.
    if (Sys.recording) {
        g.fillCircle(x + 3, 7, 3, ERR);
        g.setTextColor(ERR, PANEL);
        g.drawString(hhmmss(Sys.recSeconds).c_str(), x + 9, 4);
    } else if (Sys.focusRunning) {
        g.setTextColor(ACCENT, PANEL);
        g.drawString(hhmmss(Sys.focusRemain).c_str(), x, 4);
    } else if (host::updateReady()) {
        g.setTextColor(ACCENT, PANEL);
        g.drawString("^U upd", x, 4);
    }

    // Claude Code status light (Core /buddy): amber = waiting on you, green = working.
    if (Sys.buddy) g.fillCircle(104, 7, 3, Sys.buddy == 2 ? WARN : OK);

    // Right side, laid out from the edge inwards.
    batteryGlyph(g, SCREEN_W - 24, 3, Sys.batteryPct < 0 ? 0 : Sys.batteryPct,
                 Sys.charging);
    if (Sys.batteryPct >= 0) {
        char b[6];
        snprintf(b, sizeof(b), "%d%%", Sys.batteryPct);
        g.setTextDatum(top_right);
        g.setTextColor(DIM, PANEL);
        g.drawString(b, SCREEN_W - 28, 4);
        g.setTextDatum(top_left);
    }

    int rx = SCREEN_W - 56;
    if (Sys.storage == Storage::SD) {
        g.setTextColor(DIM, PANEL);
        g.drawString("SD", rx, 4);
        rx -= 16;
    }
    if (Sys.wifiConnected) {
        g.setTextColor(Sys.hostOnline ? OK : ACCENT2, PANEL);
        g.drawString("WiFi", rx - 8, 4);
    } else if (Sys.wifiOn) {
        g.setTextColor(WARN, PANEL);
        g.drawString("WiFi", rx - 8, 4);
    }
}

void hintBar(M5Canvas& g, const char* hints) {
    const int y = SCREEN_H - HINT_H;
    g.fillRect(0, y, SCREEN_W, HINT_H, PANEL);
    g.drawFastHLine(0, y, SCREEN_W, LINE);
    g.setFont(&fonts::Font0);
    g.setTextDatum(top_left);

    // The back affordance. This device has no pointer, so "back button" has to
    // mean a fixed, always-present label of the key that goes back. Filled
    // amber when there is somewhere to return to, hairline grey on Home when
    // there is not — a control that vanishes is a control you stop trusting.
    const bool canBack = Sys.navDepth > 1;
    g.fillRoundRect(2, y + 1, 32, 11, 2, canBack ? ACCENT : LINE);
    g.setTextColor(canBack ? BG : DIM, canBack ? ACCENT : LINE);
    g.drawString("<ESC", 6, y + 3);
    // Two or more deep: a tick past the chip says there is still more behind
    // this screen, so you hold ESC for Home rather than mashing it.
    if (Sys.navDepth > 2) g.fillRect(36, y + 1, 2, 11, ACCENT);

    // 33 chars is what fits beside the chip at Font0's 6px advance. Hints used
    // to run off the right edge silently; now they are cut where they land.
    char buf[34];
    snprintf(buf, sizeof(buf), "%.33s", hints ? hints : "");
    g.setTextColor(HINT, PANEL);
    g.drawString(buf, 41, y + 3);
}

void header(M5Canvas& g, const char* title, const char* right) {
    g.setFont(&fonts::Font2);
    g.setTextDatum(top_left);
    // The chip says how to go back; this chevron says there is a back at all,
    // inside the body where the eye already is.
    const int titleX = Sys.navDepth > 1 ? PAD + 10 : PAD;
    if (Sys.navDepth > 1) {
        g.setTextColor(DIM, BG);
        g.drawString("<", PAD, BODY_Y + 2);
    }
    g.setTextColor(ACCENT, BG);
    g.drawString(title, titleX, BODY_Y + 2);
    if (right) {
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.setTextDatum(top_right);
        g.drawString(right, SCREEN_W - PAD, BODY_Y + 7);
        g.setTextDatum(top_left);
    }
    g.drawFastHLine(PAD, BODY_Y + 19, SCREEN_W - PAD * 2, LINE);
}

// ----------------------------------------------------------------- widgets
void panel(M5Canvas& g, int x, int y, int w, int h, uint16_t fill) {
    g.fillRoundRect(x, y, w, h, 3, fill);
}

void listRow(M5Canvas& g, int visibleIndex, bool selected, const char* label,
             const char* right) {
    const int y = BODY_Y + 1 + visibleIndex * ROW_H;
    if (selected) {
        g.fillRoundRect(2, y, SCREEN_W - 8, ROW_H - 2, 3, ACCENT);
        g.fillRect(2, y, 3, ROW_H - 2, TEXT);
    }
    g.setFont(&fonts::Font2);
    g.setTextDatum(top_left);
    g.setTextColor(selected ? BG : TEXT, selected ? ACCENT : BG);
    g.drawString(label, 9, y + 1);
    if (right && *right) {
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_right);
        g.setTextColor(selected ? BG : DIM, selected ? ACCENT : BG);
        g.drawString(right, SCREEN_W - 12, y + 5);
        g.setTextDatum(top_left);
    }
}

void scrollBar(M5Canvas& g, int total, int firstVisible, int visible) {
    if (total <= visible) return;
    const int trackH = BODY_H - 4;
    const int h      = trackH * visible / total;
    const int y = BODY_Y + 2 + (trackH - h) * firstVisible / (total - visible);
    g.fillRect(SCREEN_W - 4, BODY_Y + 2, 2, trackH, PANEL);
    g.fillRect(SCREEN_W - 4, y, 2, h < 6 ? 6 : h, ACCENT);
}

void emptyState(M5Canvas& g, const char* line1, const char* line2) {
    g.setTextDatum(middle_center);
    g.setFont(&fonts::Font2);
    g.setTextColor(DIM, BG);
    g.drawString(line1, SCREEN_W / 2, BODY_Y + BODY_H / 2 - (line2 ? 9 : 0));
    if (line2) {
        g.setFont(&fonts::Font0);
        g.drawString(line2, SCREEN_W / 2, BODY_Y + BODY_H / 2 + 12);
    }
    g.setTextDatum(top_left);
}

void bigValue(M5Canvas& g, const char* value, const char* caption,
              uint16_t colour) {
    g.setTextDatum(middle_center);
    g.setFont(&fonts::Font7);
    g.setTextColor(colour, BG);
    g.drawString(value, SCREEN_W / 2, BODY_Y + BODY_H / 2 - 6);
    if (caption) {
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.drawString(caption, SCREEN_W / 2, BODY_Y + BODY_H - 10);
    }
    g.setTextDatum(top_left);
}

void progress(M5Canvas& g, int x, int y, int w, int h, float pct,
              uint16_t colour) {
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;
    g.fillRoundRect(x, y, w, h, h / 2, PANEL);
    const int fw = static_cast<int>(w * pct);
    if (fw > 1) g.fillRoundRect(x, y, fw, h, h / 2, colour);
}

// -------------------------------------------------------------------- text
std::string ellipsis(const std::string& s, size_t maxChars) {
    if (s.size() <= maxChars) return s;
    if (maxChars <= 1) return std::string(maxChars, '.');
    return s.substr(0, maxChars - 1) + "~";
}

std::string hhmmss(uint32_t seconds) {
    char buf[16];
    if (seconds >= 3600)
        snprintf(buf, sizeof(buf), "%u:%02u:%02u", seconds / 3600,
                 (seconds / 60) % 60, seconds % 60);
    else
        snprintf(buf, sizeof(buf), "%02u:%02u", seconds / 60, seconds % 60);
    return buf;
}

std::string humanSize(size_t bytes) {
    char buf[16];
    if (bytes >= 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1fMB", bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        snprintf(buf, sizeof(buf), "%uKB", (unsigned)(bytes / 1024));
    else
        snprintf(buf, sizeof(buf), "%uB", (unsigned)bytes);
    return buf;
}

std::string stamp(uint32_t epoch) {
    if (epoch == 0) return "--";
    time_t    t = static_cast<time_t>(epoch);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", tmv.tm_year + 1900,
             tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
    return buf;
}

}  // namespace ui
}  // namespace maz
