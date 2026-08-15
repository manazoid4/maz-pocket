// MAZ Pocket COMM + OPS product surfaces.
#include <algorithm>
#include <string>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/notify.h"
#include "../core/settings.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../net/action_ids.generated.h"
#include "../net/comm_stream.h"
#include "../net/host_async.h"
#include "../net/mazhost.h"
#include "../storage/store.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

std::string gCallSession;

void streamPcmTap(const int16_t* samples, size_t count) {
    comm_stream::pushPcm(samples, count);
}

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
    {action_ids::PC_DESKTOP,     "DESKTOP", "show / hide desktop"},
    {action_ids::PC_PLAY_PAUSE,  "PLAY",    "play or pause media"},
    {action_ids::PC_MUTE,        "MUTE",    "toggle PC sound"},
    {action_ids::PC_VOLUME_DOWN, "VOL -",   "lower volume"},
    {action_ids::PC_VOLUME_UP,   "VOL +",   "raise volume"},
    {action_ids::PC_LOCK,        "LOCK",    "lock Windows"},
};
constexpr int CONTROL_COUNT = sizeof(CONTROLS) / sizeof(CONTROLS[0]);

class CallPCApp final : public App {
public:
    const char* id() const override { return "talk"; }
    const char* title() const override { return "Call PC"; }

    const char* hints() const override {
        if (_controlMode) return "< > choose   ENTER send   C voice";
        if (voice::state() == voice::State::Listening) return "release SPACE to send";
        if (_sending) return "SPACE cancel   N new line";
        if (voice::isPlaying()) return "SPACE interrupt   P replay";
        if (!_reply.empty()) return "SPACE reply   C controls   N new";
        return "hold SPACE call   C controls   N new";
    }

    void onEnter() override {
        _sending = false;
        _usingStream = false;
        _scroll = 0;
        _controlMode = false;
        _queuedTake = false;
        voice::setCaptureTap(nullptr);
        if (KB.held(KEY_SPACE)) beginTake();
        invalidate();
    }

    void onExit() override {
        voice::setCaptureTap(nullptr);
        if (voice::state() == voice::State::Listening) voice::stop();
        voice::stopPlayback();
        comm_stream::cancel();
        if (_restReq) host_async::cancel(_restReq);
        if (_controlReq) host_async::cancel(_controlReq);
        if (_ttsReq) host_async::cancel(_ttsReq);
        if (_sending && !_takePath.empty())
            queueTake("screen closed before host finished");
        else
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
            if (voice::isPlaying()) {
                voice::stopPlayback();
                return true;
            }
            if (_sending) {
                cancelPending(true);
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

        if (e.code == KEY_C && !_sending && voice::state() != voice::State::Listening) {
            _controlMode = true;
            voice::stopPlayback();
            sfx::select();
            invalidate();
            return true;
        }
        if (e.code == KEY_N) {
            cancelPending(false);
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
        pumpStream();
        pumpRest();
        pumpControl();
        pumpTts();
        if (voice::state() == voice::State::Listening || voice::isPlaying() || _sending)
            invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        if (_controlMode) {
            renderControl(g);
            return;
        }

        const char* link = _usingStream ? "WS LIVE" : host::linkName();
        ui::header(g, "COMM / PC", link);

        if (voice::state() == voice::State::Listening) {
            ui::panel(g, 71, BODY_Y + 19, 98, 52);
            g.setTextDatum(middle_center);
            g.setFont(&fonts::Font4);
            g.setTextColor(ACCENT2, PANEL);
            g.drawString("REC", SCREEN_W / 2, BODY_Y + 44);
            g.setTextDatum(top_left);
            g.setFont(&fonts::Font0);
            g.setTextColor(DIM, BG);
            const char* path = _usingStream ? "WS+SD" : "SD";
            g.drawString((std::string(path) + " 00:" + two(voice::elapsedSeconds())).c_str(),
                         91, BODY_Y + 78);
            return;
        }

        if (_sending && _reply.empty()) {
            retroPhone(g, _usingStream ? "STREAMING" : "REST FALLBACK", WARN);
            return;
        }
        if (voice::isPlaying()) {
            retroPhone(g, "PC TALKING", OK);
            return;
        }
        if (!_reply.empty()) {
            g.setFont(&fonts::Font0);
            g.setTextColor(ACCENT, BG);
            const std::string mode = _usingStream ? "STREAM" : host::linkName();
            g.drawString((std::string("PC> ") + mode + " / " + routeName()).c_str(),
                         PAD, BODY_Y + 18);
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
    const char* routeWire() const {
        return Cfg.talkRoute == 0 ? "local" : (Cfg.talkRoute == 2 ? "cloud" : "auto");
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
        ui::header(g, "COMMAND DECK", _controlReq ? "SENDING" : host::linkName());
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
        if (_controlReq) {
            notify::post(Note::Info, "Command pending", "wait for PC acknowledgement");
            return;
        }
        _controlReq = host_async::pcAction(CONTROLS[_controlSel].key);
        if (!_controlReq) {
            sfx::error();
            notify::post(Note::Warn, "Host queue busy", "try again in a moment");
        }
        invalidate();
    }

    void pumpControl() {
        if (!_controlReq) return;
        host_async::Result result;
        if (!host_async::poll(_controlReq, result)) return;
        _controlReq = 0;
        const auto& control = CONTROLS[_controlSel];
        if (!result.cancelled && result.reply.ok) {
            sfx::confirm();
            notify::post(Note::Success, control.label,
                         result.reply.text.empty() ? "PC acknowledged" : result.reply.text);
        } else if (!result.cancelled) {
            sfx::error();
            notify::post(Note::Error, "Command failed",
                         result.error.empty() ? "PC unavailable" : result.error);
        }
        invalidate();
    }

    void beginTake() {
        discardTake();
        _reply.clear();
        _queuedTake = false;
        _usingStream = comm_stream::start(gCallSession, routeWire());
        voice::setCaptureTap(_usingStream ? streamPcmTap : nullptr);
        _sink = new voice::WavFileSink("outbox");
        if (!voice::start(_sink, 60)) {
            voice::setCaptureTap(nullptr);
            comm_stream::cancel();
            _usingStream = false;
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
            voice::setCaptureTap(nullptr);
            comm_stream::cancel();
            _usingStream = false;
            notify::post(Note::Error, "Recording failed", voice::lastError());
            delete _sink;
            _sink = nullptr;
            return;
        }
        voice::setCaptureTap(nullptr);
        sfx::recStop();
        _takePath = _sink->path();
        delete _sink;
        _sink = nullptr;
        _sending = true;

        if (_usingStream && !comm_stream::failed())
            comm_stream::finish();
        else
            startRestFallback("stream unavailable");
        invalidate();
    }

    void pumpStream() {
        if (!_usingStream) return;
        comm_stream::Event event;
        while (comm_stream::poll(event)) {
            if (!event.sessionId.empty()) gCallSession = event.sessionId;
            switch (event.type) {
                case comm_stream::EventType::Ready:
                    break;
                case comm_stream::EventType::Transcript:
                    _transcript = event.text;
                    break;
                case comm_stream::EventType::Delta:
                    _reply += event.text;
                    _scroll = 0;
                    break;
                case comm_stream::EventType::Done:
                    finishReply(event.text.empty() ? _reply : event.text,
                                event.provider.empty() ? "ws" : event.provider);
                    _usingStream = false;
                    _sending = false;
                    return;
                case comm_stream::EventType::Error:
                case comm_stream::EventType::Disconnected:
                    startRestFallback(event.text.empty() ? "stream lost" : event.text);
                    return;
                case comm_stream::EventType::Cancelled:
                    _usingStream = false;
                    _sending = false;
                    return;
            }
        }
        if (_usingStream && comm_stream::failed()) startRestFallback("stream failed");
    }

    void startRestFallback(const std::string& reason) {
        if (!_usingStream && _restReq) return;
        comm_stream::cancel();
        _usingStream = false;
        _reply.clear();
        if (_takePath.empty()) {
            _sending = false;
            notify::post(Note::Error, "No saved turn", reason);
            return;
        }

        // WS and REST deliberately have independent server-side session stores
        // in v0.5.1. Never send a WS session ID to REST after transport failure:
        // preserve the complete saved turn and start a fresh REST line instead.
        gCallSession.clear();
        _restReq = host_async::talkAudio("", _takePath);
        if (!_restReq) {
            _sending = false;
            queueTake("host queue busy");
            notify::post(Note::Warn, "Queued offline", "raw WAV kept on SD");
            return;
        }
        _sending = true;
        _fallbackReason = reason;
        invalidate();
    }

    void pumpRest() {
        if (!_restReq) return;
        host_async::Result result;
        if (!host_async::poll(_restReq, result)) return;
        _restReq = 0;
        _sending = false;
        if (result.cancelled) return;
        if (!result.session.empty()) gCallSession = result.session;
        if (!result.reply.ok) {
            queueTake(result.error.empty() ? "PC unavailable" : result.error);
            notify::post(Note::Warn, "PC unavailable", "voice turn queued on SD");
            invalidate();
            return;
        }
        finishReply(result.reply.text, result.reply.provider);
    }

    void finishReply(const std::string& text, const std::string& provider) {
        _reply = text;
        _scroll = 0;
        store::Record answer;
        answer.kind = "inbox";
        answer.status = "open";
        answer.title = "Call PC";
        answer.body = _reply;
        answer.source = provider;
        store::addRecord(answer);
        discardTake();

        if (Cfg.ttsEnabled && store::ready() && !_reply.empty()) {
            if (!_speechPath.empty()) store::remove(_speechPath);
            _speechPath = store::newPath("cache", "wav");
            _ttsReq = host_async::speak(_reply, _speechPath);
            if (!_ttsReq) _speechPath.clear();
        }
        sfx::confirm();
        invalidate();
    }

    void pumpTts() {
        if (!_ttsReq) return;
        host_async::Result result;
        if (!host_async::poll(_ttsReq, result)) return;
        _ttsReq = 0;
        if (!result.cancelled && result.boolValue && !_speechPath.empty())
            voice::play(_speechPath);
        else if (!result.boolValue)
            _speechPath.clear();
    }

    void cancelPending(bool keepRaw) {
        comm_stream::cancel();
        _usingStream = false;
        if (_restReq) host_async::cancel(_restReq);
        _restReq = 0;
        _sending = false;
        if (keepRaw && !_takePath.empty()) queueTake("cancelled by Maz");
        notify::post(Note::Info, "Turn cancelled", keepRaw ? "raw WAV kept" : "line reset");
        invalidate();
    }

    void queueTake(const std::string& reason) {
        if (_takePath.empty() || _queuedTake) return;
        store::Record queued;
        queued.kind = "outbox";
        queued.status = "queued";
        queued.title = "Call PC turn";
        queued.body = reason;
        queued.source = "talk";
        queued.ref = _takePath;
        store::addRecord(queued);
        _queuedTake = true;
        _takePath.clear();
    }

    void discardTake() {
        if (!_takePath.empty()) store::remove(_takePath);
        _takePath.clear();
        _queuedTake = false;
    }

    voice::WavFileSink* _sink = nullptr;
    std::string _takePath;
    std::string _speechPath;
    std::string _reply;
    std::string _transcript;
    std::string _fallbackReason;
    int _scroll = 0;
    int _controlSel = 0;
    bool _controlMode = false;
    bool _sending = false;
    bool _usingStream = false;
    bool _queuedTake = false;
    uint32_t _restReq = 0;
    uint32_t _controlReq = 0;
    uint32_t _ttsReq = 0;
};

class AgentsApp final : public App {
public:
    const char* id() const override { return "nudge"; }
    const char* title() const override { return "Agents"; }
    const char* hints() const override {
        if (_detail) return "N nudge   ESC list";
        return _refreshReq ? "refreshing...   ESC back"
                           : "ENTER inspect   N nudge   R refresh";
    }

    void onEnter() override { requestRefresh(); }
    void onExit() override {
        if (_refreshReq) host_async::cancel(_refreshReq);
        if (_nudgeReq) host_async::cancel(_nudgeReq);
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_R) {
            if (_refreshReq) host_async::cancel(_refreshReq);
            _refreshReq = 0;
            requestRefresh();
            return true;
        }
        if (_detail && e.code == KEY_ESC) {
            _detail = false;
            invalidate();
            return true;
        }
        if (_cursor.onKey(e, static_cast<int>(_summary.agents.size()))) {
            invalidate();
            return true;
        }
        if (_summary.agents.empty()) return false;
        if (e.code == KEY_ENTER) {
            _detail = !_detail;
            invalidate();
            return true;
        }
        if (e.code == KEY_N && !_nudgeReq) {
            _nudgeReq = host_async::sendNudge(_summary.agents[_cursor.sel].id);
            if (_nudgeReq)
                notify::post(Note::Info, "Nudge queued", _summary.agents[_cursor.sel].name);
            else
                notify::post(Note::Warn, "Host queue busy", "try again shortly");
            return true;
        }
        return false;
    }

    void update() override {
        if (_refreshReq) {
            host_async::Result result;
            if (host_async::poll(_refreshReq, result)) {
                _refreshReq = 0;
                if (!result.cancelled) applySummary(result.assurance);
            }
        }
        if (_nudgeReq) {
            host_async::Result result;
            if (host_async::poll(_nudgeReq, result)) {
                _nudgeReq = 0;
                if (!result.cancelled) {
                    notify::post(result.reply.ok ? Note::Success : Note::Error,
                                 result.reply.ok ? "Agent nudged" : "Nudge failed",
                                 result.reply.ok ? "request delivered" : result.error);
                    requestRefresh();
                }
            }
        }
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "OPS / AGENTS",
                   _refreshReq ? "UPDATING" : (_summary.ok ? host::linkName() : "OFFLINE"));
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
            ui::emptyState(g,
                           _refreshReq ? "Checking agents..."
                                       : (_summary.ok ? "No active agents" : "PC unavailable"),
                           _refreshReq ? "device remains responsive"
                                       : (_summary.ok ? "Agent Nudge has nothing pending"
                                                      : "Call PC or check connection"));
    }

private:
    void requestRefresh() {
        if (_refreshReq) return;
        _status = "CHECKING";
        _refreshReq = host_async::assurance();
        if (!_refreshReq) _status = "HOST BUSY";
        invalidate();
    }

    void applySummary(const host::Assurance& summary) {
        _summary = summary;
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
    uint32_t _refreshReq = 0;
    uint32_t _nudgeReq = 0;
};

}  // namespace

App* makeComm() { return new CallPCApp(); }
App* makeOps() { return new AgentsApp(); }

}  // namespace apps
}  // namespace maz
