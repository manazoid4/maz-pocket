// MAZ Pocket v0.3 showcase extras.
// Deliberately tiny and hidden from Home: one proper game and one hardware
// demo. They exist to make the device fun to hand someone without turning the
// product into an ESP32 junk drawer.
#include <M5Unified.h>
#include <esp_system.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "../audio/sfx.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

// ------------------------------------------------------------------ Snake
class SnakeApp : public App {
public:
    const char* id() const override { return "snake"; }
    const char* title() const override { return "Snake"; }
    const char* hints() const override {
        return _dead ? "ENTER restart   ESC back" : "arrows move   SPACE pause";
    }

    void onEnter() override { reset(); }

    bool onKey(const KeyEvent& e) override {
        if (e.code == KEY_SPACE) {
            if (e.down && !_dead) {
                _paused = !_paused;
                sfx::select();
                invalidate();
            }
            return true;
        }
        if (!e.down) return false;
        if (_dead && e.code == KEY_ENTER) {
            reset();
            return true;
        }
        int ndx = _dx, ndy = _dy;
        if (e.code == KEY_LEFT)  { ndx = -1; ndy = 0; }
        if (e.code == KEY_RIGHT) { ndx =  1; ndy = 0; }
        if (e.code == KEY_UP)    { ndx = 0; ndy = -1; }
        if (e.code == KEY_DOWN)  { ndx = 0; ndy =  1; }
        // Never allow an instant 180-degree turn into yourself.
        if (ndx != _dx || ndy != _dy) {
            if (!(ndx == -_dx && ndy == -_dy)) {
                _nextDx = ndx;
                _nextDy = ndy;
            }
            return true;
        }
        return false;
    }

    void update() override {
        if (_dead || _paused) return;
        const uint32_t interval = std::max<uint32_t>(70, 145 - _score * 3);
        if (millis() - _lastStep < interval) return;
        _lastStep = millis();
        step();
        invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "SNAKE");

        char score[24];
        snprintf(score, sizeof(score), "SCORE %02u", static_cast<unsigned>(_score));
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.setTextDatum(top_right);
        g.drawString(score, SCREEN_W - PAD, BODY_Y + 17);
        g.setTextDatum(top_left);

        constexpr int ox = 20;
        constexpr int oy = BODY_Y + 29;
        constexpr int cell = 10;
        g.drawRect(ox - 2, oy - 2, W * cell + 4, H * cell + 4, LINE);

        const int fx = _food % W;
        const int fy = _food / W;
        g.fillRect(ox + fx * cell + 2, oy + fy * cell + 2, 6, 6, ACCENT2);

        for (int i = _length - 1; i >= 0; --i) {
            const int x = _snake[i] % W;
            const int y = _snake[i] / W;
            const uint16_t c = i == 0 ? ACCENT : TEXT;
            g.fillRoundRect(ox + x * cell + 1, oy + y * cell + 1, 8, 8, 2, c);
        }

        if (_paused || _dead) {
            ui::panel(g, 67, oy + 24, 106, 28);
            g.setFont(&fonts::Font2);
            g.setTextColor(_dead ? ERR : WARN, PANEL);
            g.setTextDatum(middle_center);
            g.drawString(_dead ? "GAME OVER" : "PAUSED", SCREEN_W / 2, oy + 38);
            g.setTextDatum(top_left);
        }
    }

private:
    static constexpr int W = 20;
    static constexpr int H = 8;
    static constexpr int MAX = W * H;

    void reset() {
        _length = 4;
        _snake[0] = 4 * W + 9;
        _snake[1] = 4 * W + 8;
        _snake[2] = 4 * W + 7;
        _snake[3] = 4 * W + 6;
        _dx = _nextDx = 1;
        _dy = _nextDy = 0;
        _score = 0;
        _dead = false;
        _paused = false;
        _lastStep = millis();
        spawnFood();
        invalidate();
    }

    bool occupied(uint16_t cell) const {
        for (int i = 0; i < _length; ++i)
            if (_snake[i] == cell) return true;
        return false;
    }

    void spawnFood() {
        if (_length >= MAX) {
            _dead = true;
            return;
        }
        for (int tries = 0; tries < 256; ++tries) {
            const uint16_t candidate = static_cast<uint16_t>(esp_random() % MAX);
            if (!occupied(candidate)) {
                _food = candidate;
                return;
            }
        }
        for (uint16_t i = 0; i < MAX; ++i)
            if (!occupied(i)) { _food = i; return; }
    }

    void step() {
        _dx = _nextDx;
        _dy = _nextDy;
        const int hx = _snake[0] % W;
        const int hy = _snake[0] / W;
        const int nx = hx + _dx;
        const int ny = hy + _dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) {
            die();
            return;
        }
        const uint16_t head = static_cast<uint16_t>(ny * W + nx);
        // Tail moves away this tick unless we're eating, so entering its old
        // square is legal.
        const bool eating = head == _food;
        const int collisionLen = eating ? _length : _length - 1;
        for (int i = 0; i < collisionLen; ++i) {
            if (_snake[i] == head) {
                die();
                return;
            }
        }
        if (eating && _length < MAX) ++_length;
        for (int i = _length - 1; i > 0; --i) _snake[i] = _snake[i - 1];
        _snake[0] = head;
        if (eating) {
            ++_score;
            sfx::confirm();
            spawnFood();
        }
    }

    void die() {
        _dead = true;
        sfx::error();
        invalidate();
    }

    std::array<uint16_t, MAX> _snake{};
    uint16_t _food = 0;
    int _length = 0;
    int _dx = 1, _dy = 0;
    int _nextDx = 1, _nextDy = 0;
    uint32_t _lastStep = 0;
    uint16_t _score = 0;
    bool _dead = false;
    bool _paused = false;
};

// ------------------------------------------------------------- Hyperdrive
struct Star {
    float x;
    float y;
    float z;
};

class HyperdriveApp : public App {
public:
    const char* id() const override { return "hyperdrive"; }
    const char* title() const override { return "Hyperdrive"; }
    const char* hints() const override { return "tilt device   hold SPACE boost"; }

    void onEnter() override {
        for (auto& s : _stars) resetStar(s, true);
        _last = millis();
        invalidate();
    }

    bool onKey(const KeyEvent& e) override {
        if (e.code != KEY_SPACE) return false;
        _boost = e.down;
        if (e.down) sfx::select();
        invalidate();
        return true;
    }

    void onExit() override { _boost = false; }

    void update() override {
        const uint32_t now = millis();
        if (now - _last < 32) return;
        _last = now;

        float ax = 0, ay = 0, az = 0;
        if (M5.Imu.isEnabled()) M5.Imu.getAccelData(&ax, &ay, &az);
        _tiltX = _tiltX * 0.82f + ay * 0.18f;
        _tiltY = _tiltY * 0.82f + ax * 0.18f;

        const float speed = _boost ? 0.080f : 0.026f;
        for (auto& s : _stars) {
            s.z -= speed;
            if (s.z < 0.08f) resetStar(s, false);
        }
        invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        const int cx = SCREEN_W / 2 + static_cast<int>(_tiltX * 22.f);
        const int cy = 63 + static_cast<int>(_tiltY * 16.f);

        for (const auto& s : _stars) {
            const float inv = 1.0f / s.z;
            const int x = cx + static_cast<int>(s.x * inv * 70.f);
            const int y = cy + static_cast<int>(s.y * inv * 46.f);
            if (x < 1 || x >= SCREEN_W - 1 || y < STATUS_H + 1 || y >= SCREEN_H - HINT_H - 1)
                continue;
            const int r = s.z < 0.30f ? 2 : 1;
            const uint16_t c = s.z < 0.35f ? ACCENT2 : (s.z < 0.65f ? TEXT : DIM);
            g.fillRect(x, y, r, r, c);
            if (_boost && s.z < 0.45f) {
                const int tx = cx + static_cast<int>(s.x * (1.0f / (s.z + 0.09f)) * 70.f);
                const int ty = cy + static_cast<int>(s.y * (1.0f / (s.z + 0.09f)) * 46.f);
                g.drawLine(tx, ty, x, y, c);
            }
        }

        g.setFont(&fonts::Font0);
        g.setTextColor(_boost ? ACCENT : DIM, BG);
        g.setTextDatum(top_center);
        g.drawString(_boost ? "HYPERDRIVE" : "ATTITUDE CONTROL", SCREEN_W / 2, BODY_Y + 3);
        g.setTextDatum(top_left);
    }

private:
    void resetStar(Star& s, bool spread) {
        const auto unit = []() -> float {
            return (static_cast<int32_t>(esp_random() & 0xFFFF) - 32768) / 32768.0f;
        };
        s.x = unit();
        s.y = unit();
        s.z = spread ? 0.15f + (esp_random() % 850) / 1000.0f : 1.0f;
    }

    std::array<Star, 44> _stars{};
    uint32_t _last = 0;
    float _tiltX = 0;
    float _tiltY = 0;
    bool _boost = false;
};

}  // namespace

App* makeSnake() { return new SnakeApp(); }
App* makeHyperdrive() { return new HyperdriveApp(); }

}  // namespace apps
}  // namespace maz
