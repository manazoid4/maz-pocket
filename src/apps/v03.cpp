// MAZ Pocket v0.3 — the two product surfaces that replace generic Talk/Nudge
// on Home. Capture keeps its existing proven BrainDump implementation.
#include <algorithm>
#include <array>
#include <string>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/notify.h"
#include "../core/settings.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../net/mazhost.h"
#include "../storage/store.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

std::string gCallSession;

void drawWrapped(M5Canvas& g, const std::string& text, int y, int first = 0) {
    constexpr size_t W = 37;
    g.setFont(&fonts::Font0);
    g.setTextColor(TEXT, BG);
    for (int row = 0; row < 4; ++row) {
        const size_t start = static_cast<size_t>(first + row) * W;
        if (start >= text.size()) break;
        g.drawString(text.substr(start, W).c_str(), PAD, y + row * 14);
    }
}

struct ControlAction {
    const char* key;
    const char* label;
    const char* hint;
};

constexpr ControlAction CONTROLS[] = {
    {"desktop",     "DESKTOP", "show / hide desktop"},
    {"play_pause",  "PLAY",    "play or pause media"},
    {"mute",        "MUTE",    "toggle PC sound"},
    {"volume_down", "VOL -",   "lower volume"},
    {"volume_up",   "VOL +",   "raise volume"},
    {"lock",        "LOCK",    "lock Windows"},
};
constexpr int CONTROL_COUNT = sizeof(CONTROLS) / sizeof(CONTROLS[0]);

class CallPCApp : public App {
public:
    const char* id() const override { return "talk"; }
    const char* title() const override { return "Call PC"; }

    const char* hints() const override {
        if (_controlMode) return "< > choose   ENTER send   C voice";
        if (voice::state() == voice::State::Listening) return "release SPACE to send";
        if (_sending) return "PC is thinking...";
        if (voice::isPlaying()) return "SPACE interrupt   P replay";
        if (!_reply.empty()) return "SPACE reply   C controls   N new";
        return "hold SPACE call   C controls   N new";
    }

    void onEnter() override {
        _sending = false;
        _haveTake = false;
        _scroll = 0;
        _controlMode = false;
        if (KB.held(KEY_SPACE)) beginTake();
        invalidate();
    }

    void onExit() override {
        if (voice::state() == voice::State::Listening) voice::stop();
        voice::stopPlayback();
        discardTake();
    }

    bool onKey(const KeyEvent& e) override {
        if (_controlMode) {
            if (!e.down) return false;
            if (e.code == KEY_C) {
                _controlMode = false;
                sfx::select();
                invalidate();
                return true;
            }
            if (e.code == KEY_LEFT) {
                _controlSel = (_controlSel + CONTROL_COUNT - 1) % CONTROL_COUNT;
                sfx::select();
                invalidate();
                return true;
            }
            if (e.code == KEY_RIGHT) {
                _controlSel = (_controlSel + 1) % CONTROL_COUNT;
                sfx::select();
                invalidate();
                return true;
            }
            if (e.code == KEY_ENTER) {
                runControl();
                return true;
            }
            if (e.code == KEY_SPACE) {
                _controlMode = false;
                beginTake();
                return true;
            }
            return false;
        }

        if (e.down && e.code == KEY_SPACE) {
            if (voice::isPlaying()) voice::stopPlayback();
            if (voice::state() != voice::State::Listening && !_sending) beginTake();
            return true;
        }
        if (!e.down && e.code == KEY_SPACE) {
            if (voice::state() == voice::State::Listening) endTake();
            return true;
        }
        if (!e.down) return false;

        if (e.code == KEY_C && !_sending && voice::state() != voice::State::Listening) {
            _controlMode = true;
            voice::stopPlayback();
            sfx::select();
            invalidate();
            return true;
        }
        if (e.code == KEY_N && !_sending) {
            gCallSession.clear();
            _reply.clear();
            voice::stopPlayback();
            notify::post(Note::Info, "Line cleared", "new conversation");
            invalidate();
            return true;
        }
        if (e.code == KEY_A && !_sending) {
            Cfg.talkRoute = (Cfg.talkRoute + 1) % 3;
            Cfg.save();
            notify::post(Note::Info, "Route", routeName());
            invalidate();
            return true;
        }
        if (e.code == KEY_P && !_speechPath.empty() && !voice::isPlaying()) {
            voice::play(_speechPath);
            return true;
        }
        if (e.code == KEY_DOWN && !_reply.empty()) {
            ++_scroll;
            invalidate();
            return true;
        }
        if (e.code == KEY_UP && _scroll > 0) {
            --_scroll;
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (_sending && millis() >= _sendAt) sendTurn();
        if (voice::state() == voice::State::Listening || voice::isPlaying()) invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        if (_controlMode) {
            renderControl(g);
            return;
        }

        ui::header(g, "COMM / PC", host::linkName());

        if (voice::state() == voice::State::Listening) {
            ui::panel(g, 71, BODY_Y + 19, 98, 52);
            g.setTextDatum(middle_center);
            g.setFont(&fonts::Font4);
            g.setTextColor(ACCENT2, PANEL);
            g.drawString("REC", SCREEN_W / 2, BODY_Y + 44);
            g.setTextDatum(top_left);
            g.setFont(&fonts::Font0);
            g.setTextColor(DIM, BG);
            g.drawString(("TX 00:" + two(voice::elapsedSeconds())).c_str(), 98, BODY_Y + 78);
            return;
        }

        if (_sending) {
            retroPhone(g, "DIALING PC", WARN);
            return;
        }
        if (voice::isPlaying()) {
            retroPhone(g, "PC TALKING", OK);
            return;
        }
        if (!_reply.empty()) {
            g.setFont(&fonts::Font0);
            g.setTextColor(ACCENT, BG);
            g.drawString((std::string("PC> ") + host::linkName() + " / " + routeName()).c_str(), PAD, BODY_Y + 18);
            drawWrapped(g, _reply, BODY_Y + 34, _scroll);
            return;
        }
        retroPhone(g, gCallSession.empty() ? "LINE READY" : "LINE OPEN", ACCENT);
    }

private:
    static std::string two(uint32_t seconds) {
        const uint32_t s = seconds % 60;
        return s < 10 ? "0" + std::to_string(s) : std::to_string(s);
    }

    const char* routeName() const {
        return Cfg.talkRoute == 0 ? "LOCAL" : (Cfg.talkRoute == 2 ? "CLOUD" : "AUTO");
    }

    void retroPhone(M5Canvas& g, const char* status, uint16_t colour) {
        ui::panel(g, 76, BODY_Y + 17, 88, 55);
        g.drawRoundRect(93, BODY_Y + 26, 54, 28, 4, colour);
        g.drawLine(100, BODY_Y + 58, 140, BODY_Y + 58, colour);
        g.setFont(&fonts::Font2);
        g.setTextColor(colour, PANEL);
        g.setTextDatum(middle_center);
        g.drawString("PC", SCREEN_W / 2, BODY_Y + 40);
        g.setTextDatum(top_center);
        g.setFont(&fonts::Font0);
        g.setTextColor(colour, BG);
        g.drawString(status, SCREEN_W / 2, BODY_Y + 82);
        g.setTextDatum(top_left);
    }

    void renderControl(M5Canvas& g) {
        ui::header(g, "COMMAND DECK", host::linkName());
        const int prev = (_controlSel + CONTROL_COUNT - 1) % CONTROL_COUNT;
        const int next = (_controlSel + 1) % CONTROL_COUNT;

        g.setTextDatum(top_center);
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.drawString(CONTROLS[prev].label, 35, BODY_Y + 31);
        g.drawString(CONTROLS[next].label, 205, BODY_Y + 31);

        ui::panel(g, 66, BODY_Y + 19, 108, 55);
        g.setFont(&fonts::Font2);
        g.setTextColor(ACCENT, PANEL);
        g.drawString(CONTROLS[_controlSel].label, SCREEN_W / 2, BODY_Y + 33);
        g.setFont(&fonts::Font0);
        g.setTextColor(TEXT, PANEL);
        g.drawString("ENTER / TRANSMIT", SCREEN_W / 2, BODY_Y + 54);

        g.setTextColor(DIM, BG);
        g.drawString(CONTROLS[_controlSel].hint, SCREEN_W / 2, BODY_Y + 83);
        g.setTextDatum(top_left);
    }

    void runControl() {
        const auto& control = CONTROLS[_controlSel];
        const auto result = host::pcAction(control.key);
        if (result.ok) {
            sfx::confirm();
            notify::post(Note::Success, control.label,
                         result.text.empty() ? "PC acknowledged" : result.text);
        } else {
            sfx::error();
            notify::post(Note::Error, "Command failed",
                         result.error.empty() ? "PC unavailable" : result.error);
        }
        invalidate();
    }

    void beginTake() {
        discardTake();
        _sink = new voice::WavFileSink("outbox");
        if (!voice::start(_sink, 60)) {
            notify::post(Note::Error, "Mic failed", voice::lastError());
            delete _sink;
            _sink = nullptr;
            return;
        }
        sfx::recStart();
        invalidate();
    }

    void endTake() {
        if (!voice::stop()) {
            notify::post(Note::Error, "Recording failed", voice::lastError());
            delete _sink;
            _sink = nullptr;
            return;
        }
        sfx::recStop();
        _takePath = _sink->path();
        _haveTake = true;
        delete _sink;
        _sink = nullptr;
        _sending = true;
        _sendAt = millis() + 120;
        invalidate();
    }

    void sendTurn() {
        _sending = false;
        if (gCallSession.empty()) gCallSession = host::startSession();
        const auto result = gCallSession.empty()
                                ? host::Reply{}
                                : host::talkAudio(gCallSession, _takePath);
        if (!result.ok) {
            store::Record queued;
            queued.kind = "outbox";
            queued.status = "queued";
            queued.title = "Call PC turn";
            queued.body = result.error.empty() ? "PC unavailable" : result.error;
            queued.source = "talk";
            queued.ref = _takePath;
            store::addRecord(queued);
            _takePath.clear();
            _haveTake = false;
            notify::post(Note::Warn, "PC unavailable", "voice turn queued");
            invalidate();
            return;
        }

        _reply = result.text;
        _scroll = 0;
        store::Record answer;
        answer.kind = "inbox";
        answer.status = "open";
        answer.title = "Call PC";
        answer.body = result.text;
        answer.source = result.provider;
        store::addRecord(answer);
        discardTake();

        if (Cfg.ttsEnabled && store::ready() && !_reply.empty()) {
            if (!_speechPath.empty()) store::remove(_speechPath);
            _speechPath = store::newPath("cache", "wav");
            if (host::speak(_reply, _speechPath)) voice::play(_speechPath);
            else _speechPath.clear();
        }
        sfx::confirm();
        invalidate();
    }

    void discardTake() {
        if (!_takePath.empty()) store::remove(_takePath);
        _takePath.clear();
        _haveTake = false;
    }

    voice::WavFileSink* _sink = nullptr;
    std::string _takePath;
    std::string _speechPath;
    std::string _reply;
    int _scroll = 0;
    int _controlSel = 0;
    bool _controlMode = false;
    bool _haveTake = false;
    bool _sending = false;
    uint32_t _sendAt = 0;
};

class AgentsV3App : public App {
public:
    const char* id() const override { return "nudge"; }
    const char* title() const override { return "Agents"; }
    const char* hints() const override {
        return _detail ? "N nudge   ESC list" : "ENTER inspect   N nudge   R refresh";
    }

    void onEnter() override { refresh(); }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_R) { refresh(); return true; }
        if (_detail && e.code == KEY_ESC) { _detail = false; invalidate(); return true; }
        if (_cursor.onKey(e, static_cast<int>(_summary.agents.size()))) {
            invalidate();
            return true;
        }
        if (_summary.agents.empty()) {
            if (e.code == KEY_ENTER && !_summary.ok) {
                shell::pushById("talk");
                return true;
            }
            return false;
        }
        if (e.code == KEY_ENTER) { _detail = !_detail; invalidate(); return true; }
        if (e.code == KEY_N) {
            const auto result = host::sendNudge(_summary.agents[_cursor.sel].id);
            notify::post(result.ok ? Note::Success : Note::Error,
                         result.ok ? "Agent nudged" : "Nudge failed",
                         result.ok ? _summary.agents[_cursor.sel].name : result.error);
            refresh();
            return true;
        }
        return false;
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "OPS / AGENTS", _summary.ok ? host::linkName() : "OFFLINE");
        g.setFont(&fonts::Font2);
        g.setTextColor(_status == "ALL CLEAR" ? OK : WARN, BG);
        g.drawString(_status.c_str(), PAD, BODY_Y + 18);

        if (_detail && !_summary.agents.empty()) {
            const auto& a = _summary.agents[_cursor.sel];
            g.setFont(&fonts::Font0);
            g.setTextColor(ACCENT, BG);
            g.drawString(ui::ellipsis(a.name, 28).c_str(), PAD, BODY_Y + 43);
            g.setTextColor(TEXT, BG);
            g.drawString((a.provider + " / " + a.sessionState).c_str(), PAD, BODY_Y + 58);
            g.setTextColor(DIM, BG);
            g.drawString(ui::ellipsis(a.evidence.empty() ? "no outstanding evidence" : a.evidence, 36).c_str(), PAD, BODY_Y + 75);
            return;
        }

        const int visible = std::min<int>(4, static_cast<int>(_summary.agents.size()));
        for (int row = 0; row < visible; ++row) {
            const int idx = _cursor.first + row;
            if (idx >= static_cast<int>(_summary.agents.size())) break;
            const auto& a = _summary.agents[idx];
            ui::listRow(g, row + 2, idx == _cursor.sel,
                        ui::ellipsis(a.name, 17).c_str(), a.state.c_str());
        }
        if (_summary.agents.empty())
            ui::emptyState(g, _summary.ok ? "No active agents" : "PC unavailable",
                           _summary.ok ? "Agent Nudge has nothing pending" : "Call PC or check connection");
    }

private:
    void refresh() {
        _summary = host::assurance();
        if (!_summary.ok) _status = "PC OFFLINE";
        else if (_summary.questionForMaz) _status = "NEEDS MAZ";
        else if (_summary.overdue) _status = std::to_string(_summary.overdue) + " OVERDUE";
        else if (_summary.needsNudge) _status = std::to_string(_summary.needsNudge) + " NUDGE DUE";
        else if (_summary.waiting) _status = std::to_string(_summary.waiting) + " WAITING";
        else if (_summary.working) _status = std::to_string(_summary.working) + " WORKING";
        else _status = "ALL CLEAR";
        _cursor.clamp(static_cast<int>(_summary.agents.size()));
        _detail = false;
        invalidate();
    }

    host::Assurance _summary;
    ListCursor _cursor;
    bool _detail = false;
    std::string _status = "PC OFFLINE";
};

}  // namespace

App* makeCallV3() { return new CallPCApp(); }
App* makeAgentsV3() { return new AgentsV3App(); }

}  // namespace apps
}  // namespace maz
