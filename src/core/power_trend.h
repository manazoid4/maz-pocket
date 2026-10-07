// nod: battery voltage trend. Pure and dependency-free so it is checked at compile time.
//
// The Cardputer ADV cannot report charging (no charger status line, no gauge), so
// we only ever say what the voltage is doing. Samples are one per SAMPLE_MS.
// Rule: compare the mean of the oldest third with the mean of the newest third
// (at most TREND_EDGE samples each). A difference of TREND_DELTA_MV or more is a
// trend; anything less is steady. Fewer than TREND_MIN samples (3 min) is unknown.
#pragma once
#include <stdint.h>

namespace maz {
namespace power {

enum class Trend : uint8_t { Unknown, Rising, Steady, Falling };

constexpr int TREND_MAX      = 30;  // 5 min of samples at 10 s
constexpr int TREND_MIN      = 18;  // 3 min before we say anything
constexpr int TREND_EDGE     = 6;   // samples averaged at each end of the window
constexpr int TREND_DELTA_MV = 15;  // ADC noise is ~5-10 mV; 1% is ~8 mV

constexpr Trend trendOf(const uint16_t* mv, int n) {
    if (n < TREND_MIN) return Trend::Unknown;
    const int k = n / 3 < TREND_EDGE ? n / 3 : TREND_EDGE;
    int a = 0, b = 0;
    for (int i = 0; i < k; ++i) {
        a += mv[i];
        b += mv[n - 1 - i];
    }
    const int d = (b - a) / k;
    return d >= TREND_DELTA_MV ? Trend::Rising : (d <= -TREND_DELTA_MV ? Trend::Falling : Trend::Steady);
}

// Compile-time self-check: a failing build is the cheapest failing test.
namespace check {
struct Series {
    uint16_t v[TREND_MAX];
};
constexpr Series make(int start, int perStep) {
    Series s{};
    for (int i = 0; i < TREND_MAX; ++i) s.v[i] = static_cast<uint16_t>(start + perStep * i);
    return s;
}
constexpr Series kRise  = make(3800, 3);
constexpr Series kFall  = make(4000, -3);
constexpr Series kFlat  = make(4000, 0);
constexpr Series kDrift = make(4000, 1);
static_assert(trendOf(kRise.v, TREND_MAX) == Trend::Rising, "rising series");
static_assert(trendOf(kFall.v, TREND_MAX) == Trend::Falling, "falling series");
static_assert(trendOf(kFlat.v, TREND_MAX) == Trend::Steady, "flat series");
static_assert(trendOf(kDrift.v, TREND_MIN) == Trend::Steady, "+1 mV per 10 s over 3 min is steady");
static_assert(trendOf(kRise.v, TREND_MIN - 1) == Trend::Unknown, "too few samples");
}  // namespace check

}  // namespace power
}  // namespace maz
