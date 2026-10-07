#include "power.h"

#include <M5Unified.h>
#include <string.h>

#include "settings.h"
#include "sys.h"

namespace maz {
namespace power {

namespace {
constexpr uint32_t SAMPLE_MS = 10000;

uint16_t gBuf[TREND_MAX];
int      gN       = 0;
uint32_t gLoadKey = 0;  // backlight + CPU speed; a change shifts the voltage, so restart the window
uint32_t gLastMs  = 0;
int      gMin     = 0;
int      gMax     = 0;
Trend    gTrend   = Trend::Unknown;
bool     gLow     = false;
bool     gOffered = false;
bool     gCpuLow  = false;
bool     gSlow    = false;
}  // namespace

Trend trend() { return gTrend; }
int   minMv() { return gMin; }
int   maxMv() { return gMax; }
bool  lowBattery() { return gLow; }

const char* trendName() {
    switch (gTrend) {
        case Trend::Rising:  return "rising";
        case Trend::Steady:  return "steady";
        case Trend::Falling: return "falling";
        default:             return "unknown";
    }
}

bool update() {
    if (gLastMs && millis() - gLastMs < SAMPLE_MS) return false;
    gLastMs = millis();

    const int mv   = M5.Power.getBatteryVoltage();
    Sys.batteryMv  = mv;
    Sys.batteryPct = M5.Power.getBatteryLevel();
    if (mv > 0) {
        if (!gMin || mv < gMin) gMin = mv;
        if (mv > gMax) gMax = mv;
        const uint32_t key =
            M5.Display.getBrightness() | (static_cast<uint32_t>(getCpuFrequencyMhz()) << 8);
        if (key != gLoadKey) {
            gLoadKey = key;
            gN       = 0;
        }
        if (gN == TREND_MAX) {
            memmove(gBuf, gBuf + 1, (TREND_MAX - 1) * sizeof(gBuf[0]));
            gN--;
        }
        gBuf[gN++] = static_cast<uint16_t>(mv);
    }
    gTrend         = trendOf(gBuf, gN);
    Sys.powerTrend = static_cast<uint8_t>(gTrend);
    Sys.charging   = gTrend == Trend::Rising;  // "charging" in JSON now only means a rising voltage

    if (Sys.batteryPct >= 0 && Sys.batteryPct <= 15 && gTrend != Trend::Rising) gLow = true;
    if (Sys.batteryPct > 20 || gTrend == Trend::Rising) gLow = false;

    if (Serial)
        Serial.printf("PWR mv=%d pct=%d trend=%s bl=%u cpu=%d\n", mv, Sys.batteryPct, trendName(),
                      (unsigned)M5.Display.getBrightness(), getCpuFrequencyMhz());

    if (gTrend == Trend::Falling) gOffered = false;
    if (gTrend == Trend::Rising && !gOffered) {
        gOffered = true;
        return true;
    }
    return false;
}

int cpuMhz() { return getCpuFrequencyMhz(); }

void cpuFull() {
    if (!gCpuLow) return;
    setCpuFrequencyMhz(240);
    gCpuLow = false;
}

void applyCpu(bool wantLow, bool busy) {
    gSlow = wantLow && !busy;  // loop yield is independent of the frequency switch
#ifndef NOD_NO_CPU_SCALING
    if (!Cfg.lowPowerCpu) wantLow = false;
    if (wantLow == gCpuLow) return;
    if (!wantLow) {
        cpuFull();
        return;
    }
    if (busy) return;
    setCpuFrequencyMhz(80);  // Wi-Fi stays up; APB stays 80 MHz so I2S/SPI clocks are unchanged
    gCpuLow = true;
#endif
}

uint16_t loopDelayMs() { return gSlow ? 25 : 2; }

}  // namespace power
}  // namespace maz
