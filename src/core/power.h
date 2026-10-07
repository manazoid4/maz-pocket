// nod: honest power state, Docked-mode CPU/backlight policy and diagnostics.
#pragma once
#include <stdint.h>

#include "power_trend.h"

namespace maz {
namespace power {

// Sample the battery (every 10 s). Returns true once per rise when the shell
// should offer Docked mode. Updates Sys.batteryPct/batteryMv/powerTrend/charging.
bool update();

Trend       trend();
const char* trendName();   // "rising" | "steady" | "falling" | "unknown"
int         minMv();       // since boot, 0 if no sample yet
int         maxMv();
bool        lowBattery();  // <= 15% and not rising; clears at 20% or on a rise

// CPU policy. All frequency changes go through these two functions.
// Lowering to 80 MHz only happens when nothing is busy; raising is always allowed.
// Build with -DNOD_NO_CPU_SCALING, or turn off Settings > Low-power CPU, to disable.
void     applyCpu(bool wantLow, bool busy);
void     cpuFull();  // call BEFORE record, playback, upload or update
int      cpuMhz();
uint16_t loopDelayMs();  // main-loop yield: 2 ms normally, 25 ms when low-power

constexpr uint8_t  DOCK_LIGHT  = 25;  // ~10% backlight while docked
constexpr uint16_t DOCK_DARK_S = 60;  // seconds after the last key before the screen goes dark

}  // namespace power
}  // namespace maz
