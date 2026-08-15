// COMM — responsive voice conversation with MAZ Core.
//
// Recording stays on the UI task because microphone feedback is immediate.
// Host session creation, WAV upload, optional TTS and allow-listed PC actions
// run on one bounded host_worker task so a sleeping PC or slow network cannot
// freeze navigation.
#include <algorithm>
#include <array>
#include <string>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/notify.h"
#include "../core/settings.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../net/host_worker.h"
#include "../net/mazhost.h"
#include "../storage/store.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

std::string gCommSession;

void drawCommWrapped(M5Canvas& g, const std::string& text, int y, int first = 0) {
    constexpr size_t WIDTH = 37;
    g.setFont(&fonts::Font0);
    g.setTextColor(TEXT, BG);
    for (int row = 0; row < 4; ++row) {
        const size_t start = static_cast<size_t>(first + row) * WIDTH;
        if (start >= text.size()) break;
        g.drawString(text.substr(start, WIDTH).c_str(), PAD, y + row * 14);
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

const char* controlLabel(const std::string& action) {
    for (const auto& item : CONTROLS)
        if (action == item.key) return item.label;
    return "PC COMMAND";
}

class CommApp : public App {
public:
    const char* id() const override { return "talk"; }
    const char* title() const override { return "COMM"; }

    const char* hints() const override {
        if (_controlMode) return "< > choose   ENTER send   C voice";
        if (voice::state() == voice::State::Listening) return "release SPACE to send";
        if (host_worker::busy()) return "PC working in background   ESC safe";
        if (voice::isPlaying()) return "SPACE interrupt   P replay";
        if (!_reply.empty()) return "SPACE reply   C controls   N new";
        return "hold SPACE call   C controls   N new";
    }

    void onEnter() override {
        _scroll = 0;
        _controlMode = false;
        _sending = host_worker::busy();
        consumeWorkerResult();
        if (KB.held(KEY_SPACE) && !host_worker::busy()) beginTake();
        invalidate();
    }

    void onExit() override {
        voice::stopPlayback();

        // App objects are deleted when popped. Never leave a WavFileSink owned
        // by a deleted CommApp. Finalise the WAV and keep it in outbox instead.
        if (_sink) {
            const bool stopped = voice::state() == voice::State::Listening
                                     ? voice::stop()
                                     : true;
            const std::string path = _sink->path();
            delete _sink;
            _sink = nullptr;
            if (stopped && !path.empty()) queueRaw(path, "COMM recording kept");
        }

        // A completed-but-unsent take is valuable data. Leaving COMM must not
        // delete it just because navigation changed.
        if (_haveTake && !_takePath.empty()) {
            queueRaw(_takePath, "COMM take kept");
            _takePath.clear();
            _haveTake = false;
        }
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
            consumeWorkerResult();
            if (host_worker::busy()) {
                notify::post(Note::Info, "PC still working", "ESC is safe; result will wait");
                return true;
            }
            if (voice::state() != voice::State::Listening) beginTake();
            return true;
        }
        if (!e.down && e.code == KEY_SPACE) {
            if (voice::state() == voice::State::Listening) endTake();
            return true;
        }
        if (!e.down) return false;

        if (e.code == KEY_C && !host_worker::busy() &&
            voice::state() != voice::State::Listening) {
            _controlMode = true;
            voice::stopPlayback();
            sfx::select();
            invalidate();
            return true;
        }
        if (e.code == KEY_N && !host_worker::busy()) {
            gCommSession.clear();
            _reply.clear();
            voice::stopPlayback();
            notify::post(Note::Info, "Line cleared", "new conversation");
            invalidate();
            return true;
        }
        if (e.code == KEY_A && !host_worker::busy()) {
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
        if (_sending && _sendAt && millis() >= _sendAt) startWorker();
        consumeWorkerResult();

        if (voice::state() == voice::State::Listening || voice::isPlaying()) {
            invalidate();
        } else if (host_worker::busy() && millis() - _lastWorkerPaint >= 250) {
            _lastWorkerPaint = millis();
            invalidate();
        }
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

        if (host_worker::busy() || _sending) {
            retroPhone(g, host_worker::state() == host_worker::State::Queued
                              ? "QUEUED FOR PC"
                              : "PC WORKING",
                       WARN);
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
            drawCommWrapped(g, _reply, BODY_Y + 34, _scroll);
            return;
        }
        retroPhone(g, gCommSession.empty() ? "LINE READY" : "LINE OPEN", ACCENT);
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
        // This control deck is intentionally tiny and allow-listed. Keep it
        // deterministic; never turn COMM into a remote shell.
        if (host_worker::busy()) {
            notify::post(Note::Info, "PC busy", "wait for current action");
            return;
        }
        const auto& control = CONTROLS[_controlSel];
        if (!host_worker::submitPcAction(control.key)) {
            notify::post(Note::Error, "Command not queued", host_worker::stateName());
            return;
        }
        _controlMode = false;
        _sending = true;
        sfx::confirm();
        invalidate();
    }

    void beginTake() {
        if (host_worker::busy()) return;
        discardTake();
        _sink = new voice::WavFileSink("outbox");
        if (!_sink || !voice::start(_sink, 60)) {
            notify::post(Note::Error, "Mic failed", voice::lastError());
            delete _sink;
            _sink = nullptr;
            return;
        }
        sfx::recStart();
        invalidate();
    }

    void endTake() {
        if (!_sink || !voice::stop()) {
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
        _sendAt = millis() + 120;  // let the sending state paint first
        invalidate();
    }

    void startWorker() {
        _sendAt = 0;
        if (!_haveTake || _takePath.empty()) {
            _sending = false;
            return;
        }

        std::string speechPath;
        if (Cfg.ttsEnabled && store::ready()) speechPath = store::newPath("cache", "wav");

        if (!host_worker::submitTalkAudio(gCommSession, _takePath, speechPath)) {
            _sending = false;
            notify::post(Note::Error, "COMM busy", host_worker::stateName());
            return;
        }

        // The worker copied the path. The WAV stays durable on storage while
        // the app is free to navigate or even be destroyed.
        _takePath.clear();
        _haveTake = false;
        _sending = true;
        invalidate();
    }

    void consumeWorkerResult() {
        if (host_worker::state() != host_worker::State::Done) {
            _sending = host_worker::busy();
            return;
        }

        if (host_worker::jobKind() == host_worker::JobKind::PcAction) {
            host_worker::PcActionResult result;
            if (!host_worker::takePcActionResult(result)) return;
            _sending = false;
            const char* label = controlLabel(result.action);
            if (result.reply.ok) {
                sfx::confirm();
                notify::post(Note::Success, label,
                             result.reply.text.empty() ? "PC acknowledged" : result.reply.text);
            } else {
                sfx::error();
                notify::post(Note::Error, "Command failed",
                             result.reply.error.empty() ? "PC unavailable" : result.reply.error);
            }
            invalidate();
            return;
        }

        host_worker::TalkResult result;
        if (!host_worker::takeTalkResult(result)) return;
        _sending = false;
        if (!result.session.empty()) gCommSession = result.session;

        if (!result.reply.ok) {
            queueRaw(result.wavPath,
                     result.reply.error.empty() ? "PC unavailable" : result.reply.error);
            notify::post(Note::Warn, "PC unavailable", "voice turn kept in outbox");
            invalidate();
            return;
        }

        _reply = result.reply.text;
        _scroll = 0;

        store::Record answer;
        answer.kind = "inbox";
        answer.status = "open";
        answer.title = "COMM / PC";
        answer.body = result.reply.text;
        answer.source = result.reply.provider;
        answer.ref = result.wavPath;
        store::addRecord(answer);

        if (!result.reply.reminderTitle.empty() && result.reply.reminderDelay) {
            store::Record reminder;
            reminder.kind = "reminder";
            reminder.status = "open";
            reminder.title = result.reply.reminderTitle;
            reminder.source = "voice";
            scheduleReminder(reminder, result.reply.reminderDelay);
            store::addRecord(reminder);
        }

        if (!result.wavPath.empty()) store::remove(result.wavPath);

        if (result.speechReady && !result.speechPath.empty()) {
            if (!_speechPath.empty() && _speechPath != result.speechPath)
                store::remove(_speechPath);
            _speechPath = result.speechPath;
            voice::play(_speechPath);
        }

        sfx::confirm();
        notify::post(Note::Success, "Answer ready",
                     result.reply.provider.empty() ? "MAZ Core" : result.reply.provider);
        invalidate();
    }

    void queueRaw(const std::string& path, const std::string& reason) {
        if (path.empty()) return;
        store::Record queued;
        queued.kind = "outbox";
        queued.status = "queued";
        queued.title = "COMM voice turn";
        queued.body = reason;
        queued.source = "talk";
        queued.ref = path;
        store::addRecord(queued);
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
    uint32_t _lastWorkerPaint = 0;
};

}  // namespace

App* makeComm() { return new CommApp(); }

}  // namespace apps
}  // namespace maz
