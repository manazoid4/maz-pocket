#include <string>

#include "../core/metrics.h"
#include "../ui/ui.h"
#include "apps.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

class RuntimeMetricsApp final : public App {
public:
    const char* id() const override { return "runtime"; }
    const char* title() const override { return "Runtime"; }
    const char* hints() const override { return "LEFT/RIGHT page   ESC back"; }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_LEFT || e.code == KEY_RIGHT) {
            _page = 1 - _page;
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (millis() - _last > 500) {
            _last = millis();
            invalidate();
        }
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        const auto m = metrics::snapshot();
        ui::header(g, "RUNTIME", _page ? "QUEUES / LATENCY" : "MEMORY / STACK");
        if (_page == 0) {
            row(g, 0, "Free heap", bytes(m.freeHeap));
            row(g, 1, "Heap floor", bytes(m.minFreeHeap));
            row(g, 2, "Largest block", bytes(m.largestFreeBlock));
            row(g, 3, "Main stack", words(m.mainStackMinWords));
            row(g, 4, "Host stack", words(m.hostStackMinWords));
            row(g, 5, "WS stack", words(m.wsStackMinWords));
        } else {
            row(g, 0, "Host queue", pair(m.hostQueueDepth, m.hostQueueHighWater));
            row(g, 1, "Host reject/drop", pair(m.hostRejected, m.hostResultDrops));
            row(g, 2, "Host max", ms(m.hostMaxLatencyMs));
            row(g, 3, "WS queue", pair(m.wsQueueDepth, m.wsQueueHighWater));
            row(g, 4, "WS sent/drop", pair(m.wsFramesSent, m.wsFrameDrops));
            row(g, 5, "First / total", msPair(m.wsFirstTokenMs, m.wsTotalMs));
        }
    }

private:
    static std::string bytes(uint32_t value) {
        return std::to_string(value / 1024) + " KB";
    }
    static std::string words(uint32_t value) {
        return std::to_string(value) + " w";
    }
    static std::string ms(uint32_t value) {
        return std::to_string(value) + " ms";
    }
    static std::string pair(uint32_t a, uint32_t b) {
        return std::to_string(a) + " / " + std::to_string(b);
    }
    static std::string msPair(uint32_t a, uint32_t b) {
        return std::to_string(a) + " / " + std::to_string(b) + " ms";
    }
    static void row(M5Canvas& g, int index, const char* label,
                    const std::string& value) {
        const int y = BODY_Y + 16 + index * 15;
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_left);
        g.setTextColor(DIM, BG);
        g.drawString(label, PAD, y);
        g.setTextDatum(top_right);
        g.setTextColor(index == 1 ? WARN : TEXT, BG);
        g.drawString(value.c_str(), SCREEN_W - PAD, y);
        g.setTextDatum(top_left);
    }

    int _page = 0;
    uint32_t _last = 0;
};

}  // namespace

App* makeRuntimeMetrics() { return new RuntimeMetricsApp(); }

}  // namespace apps
}  // namespace maz
