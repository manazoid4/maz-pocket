#include <algorithm>
#include <string>
#include <vector>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/notify.h"
#include "../core/shell.h"
#include "../net/host_async.h"
#include "../storage/store.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

void drawShortText(M5Canvas& g, const std::string& text, int y, int rows) {
    constexpr size_t width = 38;
    g.setFont(&fonts::Font0);
    g.setTextDatum(top_left);
    g.setTextColor(TEXT, BG);
    for (int row = 0; row < rows; ++row) {
        const size_t start = static_cast<size_t>(row) * width;
        if (start >= text.size()) break;
        g.drawString(text.substr(start, width).c_str(), PAD, y + row * 14);
    }
}

void drawVoiceFace(M5Canvas& g, voice::State state, const char* caption) {
    ui::panel(g, 64, BODY_Y + 12, 112, 63);
    g.setTextDatum(middle_center);
    g.setFont(&fonts::Font4);
    g.setTextColor(state == voice::State::Paused ? WARN : ACCENT2, PANEL);
    g.drawString(state == voice::State::Paused ? "PAUSE" : "REC",
                 SCREEN_W / 2, BODY_Y + 38);
    g.setTextDatum(top_center);
    g.setFont(&fonts::Font0);
    g.setTextColor(DIM, BG);
    g.drawString(caption, SCREEN_W / 2, BODY_Y + 82);
    g.setTextDatum(top_left);
}

class CaptureV51App final : public App {
public:
    const char* id() const override { return "braindump"; }
    const char* title() const override { return "BrainDump"; }

    const char* hints() const override {
        if (voice::state() == voice::State::Listening)
            return "H mark  P pause  ENTER done";
        if (voice::state() == voice::State::Paused) return "P resume  ENTER done";
        if (_processReq) return "processing on PC...  ESC safe";
        if (!_result.empty()) return "R again  P play  I inbox";
        if (_ready) return "O retry  R again  P play raw";
        return "recording saved";
    }

    void onEnter() override {
        _highlights.clear();
        _ready = false;
        _queued = false;
        _result.clear();
        _scroll = 0;
        beginVoice();
        invalidate();
    }

    void onExit() override {
        if (voice::state() == voice::State::Listening ||
            voice::state() == voice::State::Paused)
            endVoice(false);
        if (_processReq) {
            host_async::cancel(_processReq);
            _processReq = 0;
            queueOffline("screen closed while processing");
        }
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_H && voice::state() == voice::State::Listening) {
            _highlights.push_back(voice::elapsedSeconds());
            notify::post(Note::Success, "Highlighted", ui::hhmmss(voice::elapsedSeconds()));
            return true;
        }
        if (e.code == KEY_P && voice::state() == voice::State::Listening) {
            voice::pause(); invalidate(); return true;
        }
        if (e.code == KEY_P && voice::state() == voice::State::Paused) {
            voice::resume(); invalidate(); return true;
        }
        if (e.code == KEY_ENTER &&
            (voice::state() == voice::State::Listening || voice::state() == voice::State::Paused)) {
            endVoice(true); return true;
        }
        if (e.code == KEY_P && _ready) { voice::play(_path); return true; }
        if (e.code == KEY_O && _ready && !_processReq) { submitProcess(); return true; }
        if (e.code == KEY_R && !_processReq && voice::state() != voice::State::Listening) {
            onEnter(); return true;
        }
        if (e.code == KEY_I && !_result.empty()) {
            shell::pushById("inbox"); return true;
        }
        if (!_result.empty()) {
            if (e.code == KEY_DOWN) { ++_scroll; invalidate(); return true; }
            if (e.code == KEY_UP) { if (_scroll) --_scroll; invalidate(); return true; }
        }
        return false;
    }

    void update() override {
        pumpProcess();
        if (voice::state() == voice::State::Listening || _processReq) invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "BrainDump", store::backendName());

        if (voice::state() == voice::State::Listening ||
            voice::state() == voice::State::Paused) {
            std::string caption = ui::hhmmss(voice::elapsedSeconds());
            if (!_highlights.empty())
                caption += "   " + std::to_string(_highlights.size()) + " marked";
            drawVoiceFace(g, voice::state(), caption.c_str());
            return;
        }

        if (!_result.empty()) {
            g.setFont(&fonts::Font0);
            g.setTextColor(DIM, BG);
            g.drawString(_resultOk ? "USEFUL OUTPUT" : "QUEUED - LAPTOP OFFLINE",
                         PAD, BODY_Y + 22);
            drawShortText(g,
                          _result.substr(std::min(_result.size(), _scroll * size_t(38))),
                          BODY_Y + 36, 5);
            return;
        }

        ui::emptyState(
            g,
            _processReq ? "Processing on PC" : (_ready ? "Raw thought kept" : "Ready"),
            _processReq ? "UI stays live / WAV stays on SD"
                        : (_ready ? "R records another" : "recording starts immediately"));
    }

private:
    void beginVoice() {
        voice::setCaptureTap(nullptr);
        _sink = new voice::WavFileSink("braindumps");
        if (!voice::start(_sink, 300)) {
            notify::post(Note::Error, "Cannot record", voice::lastError());
            delete _sink;
            _sink = nullptr;
            return;
        }
        sfx::recStart();
        invalidate();
    }

    void endVoice(bool processAfter) {
        const bool ok = voice::stop();
        const size_t bytes = _sink ? _sink->samples() * 2 : 0;
        if (_sink) _path = _sink->path();
        sfx::recStop();
        if (ok)
            notify::post(Note::Success, "BrainDump saved", ui::humanSize(bytes));
        else
            notify::post(Note::Error, "Nothing captured", voice::lastError());
        delete _sink;
        _sink = nullptr;
        _ready = ok;
        if (ok && processAfter) submitProcess();
        invalidate();
    }

    void submitProcess() {
        if (!_ready || _path.empty() || _processReq) return;
        _processReq = host_async::brainDump(_path, _highlights);
        if (!_processReq) {
            queueOffline("host queue busy");
            _resultOk = false;
            _result = "raw audio queued; PC busy";
            notify::post(Note::Warn, "Queued offline", "raw audio kept on device");
        }
        invalidate();
    }

    void pumpProcess() {
        if (!_processReq) return;
        host_async::Result result;
        if (!host_async::poll(_processReq, result)) return;
        _processReq = 0;
        if (result.cancelled) return;

        _resultOk = result.reply.ok;
        _result = result.reply.ok ? result.reply.text : result.error;
        if (_result.empty())
            _result = result.reply.ok ? "(nothing came back)" : "raw audio kept on device";
        _scroll = 0;

        store::Record item;
        item.kind = result.reply.ok ? "inbox" : "outbox";
        item.status = result.reply.ok ? "open" : "queued";
        item.title = result.reply.ok ? "BrainDump processed" : "BrainDump queued";
        item.body = _result;
        item.source = result.reply.ok ? result.reply.provider : "braindump";
        item.ref = _path;
        store::addRecord(item);
        if (!result.reply.ok) _queued = true;

        notify::post(result.reply.ok ? Note::Success : Note::Warn,
                     result.reply.ok ? "Useful output ready" : "Queued offline",
                     result.reply.ok ? result.reply.provider : "raw audio kept");
        invalidate();
    }

    void queueOffline(const std::string& reason) {
        if (_queued || _path.empty()) return;
        store::Record item;
        item.kind = "outbox";
        item.status = "queued";
        item.title = "BrainDump queued";
        item.body = reason;
        item.source = "braindump";
        item.ref = _path;
        store::addRecord(item);
        _queued = true;
    }

    voice::WavFileSink* _sink = nullptr;
    std::string _path;
    std::vector<uint32_t> _highlights;
    std::string _result;
    bool _resultOk = false;
    size_t _scroll = 0;
    bool _ready = false;
    bool _queued = false;
    uint32_t _processReq = 0;
};

}  // namespace

App* makeCaptureV51() { return new CaptureV51App(); }

}  // namespace apps
}  // namespace maz
