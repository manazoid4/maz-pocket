// Call, Capture and Recorder — the three voice surfaces.
//
// They share one recording engine (audio/voice) and differ only in intent:
//   Call     — a conversation turn. Short cap, review then keep or bin.
//              This is the screen v0.2 rewires to OpenFlowKit.
//   Capture  — a thought. Fastest path from pocket to stored, voice or text.
//   Recorder — a session. Long-form, file management, playback.
#include <algorithm>
#include <time.h>
#include <vector>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/notify.h"
#include "../core/shell.h"
#include "../core/settings.h"
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

const char* stateName(voice::State s) {
    switch (s) {
        case voice::State::Listening: return "LISTENING";
        case voice::State::Paused:    return "PAUSED";
        case voice::State::Saving:    return "SAVING";
        case voice::State::Playing:   return "PLAYING";
        case voice::State::Error:     return "ERROR";
        default:                      return "READY";
    }
}

uint16_t stateColour(voice::State s) {
    switch (s) {
        case voice::State::Listening: return ACCENT2;
        case voice::State::Playing:   return OK;
        case voice::State::Error:     return ERR;
        default:                      return DIM;
    }
}

// The shared voice face: state word, the MAZ mark breathing with input level,
// and a clipping warning. Every voice screen looks like this so the device
// feels like one thing.
void drawVoiceFace(M5Canvas& g, voice::State st, const char* caption) {
    const bool live = st == voice::State::Listening;
    ui::mark(g, SCREEN_W / 2, BODY_Y + 42, 16,
             live ? ACCENT2 : (st == voice::State::Playing ? OK : ACCENT),
             live ? voice::level() : 0.f);

    g.setTextDatum(top_center);
    g.setFont(&fonts::Font2);
    g.setTextColor(stateColour(st), BG);
    g.drawString(stateName(st), SCREEN_W / 2, BODY_Y + 66);

    g.setFont(&fonts::Font0);
    g.setTextColor(DIM, BG);
    if (live && voice::clipped()) {
        g.setTextColor(WARN, BG);
        g.drawString("too loud - back off the mic", SCREEN_W / 2, BODY_Y + 86);
    } else if (caption && *caption) {
        g.drawString(caption, SCREEN_W / 2, BODY_Y + 86);
    }
    g.setTextDatum(top_left);
}

void drawShortText(M5Canvas& g, const std::string& text, int y, int lines = 4) {
    g.setFont(&fonts::Font0);
    g.setTextColor(TEXT, BG);
    g.setTextDatum(top_left);
    constexpr size_t width = 38;
    for (int line = 0; line < lines; ++line) {
        const size_t start = line * width;
        if (start >= text.size()) break;
        g.drawString(text.substr(start, width).c_str(), PAD, y + line * 14);
    }
}

// ------------------------------------------------------------------- Call
class CallApp : public App {
public:
    const char* id() const override { return "talk"; }
    const char* title() const override { return "MAZ Talk"; }

    const char* hints() const override {
        if (voice::state() == voice::State::Listening)
            return "release SPACE to stop";
        if (_sending) return "sending to MAZ Host...";
        if (!_reply.empty()) return "SPACE ask again   A route   I inbox";
        if (_haveTake) return "ENTER send   P play   S save raw";
        return "hold SPACE to talk   ESC back";
    }

    void onEnter() override {
        _haveTake = false;
        _sink     = nullptr;
        _session.clear();
        _reply.clear();
        if (KB.held(KEY_SPACE)) beginTake();
        invalidate();
    }

    void onExit() override {
        voice::stopPlayback();
        if (voice::state() == voice::State::Listening) voice::stop();
        // Failed/offline turns stay in outbox; successful turns are removed.
    }

    bool onKey(const KeyEvent& e) override {
        if (e.down && e.code == KEY_SPACE) {
            if (voice::state() != voice::State::Listening) beginTake();
            return true;
        }
        if (!e.down && e.code == KEY_SPACE) {
            if (voice::state() == voice::State::Listening) endTake();
            return true;
        }
        if (!e.down) return false;

        if (e.code == KEY_A) {
            Cfg.talkRoute = (Cfg.talkRoute + 1) % 3;
            Cfg.save();
            notify::post(Note::Info, "Talk route",
                         Cfg.talkRoute == 0 ? "LOCAL" : (Cfg.talkRoute == 2 ? "CLOUD" : "AUTO"));
            invalidate();
            return true;
        }
        if (e.code == KEY_I && !_reply.empty()) { shell::pushById("inbox"); return true; }

        if (_haveTake && e.code == KEY_P) {
            voice::play(_takePath);
            invalidate();
            return true;
        }
        if (_haveTake && e.code == KEY_ENTER) { _sending = true; invalidate(); return true; }
        if (_haveTake && e.code == KEY_S) {
            keepTake();
            return true;
        }
        if (_haveTake && e.code == KEY_D) {
            discardTake();
            notify::post(Note::Info, "Deleted", "take discarded");
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (_sending && millis() >= _sendAt) {
            _sending = false;
            if (_session.empty()) _session = host::startSession();
            const auto result = _session.empty() ? host::Reply{} : host::talkAudio(_session, _takePath);
            if (result.ok) {
                _reply = result.text;
                store::Record item;
                item.kind = "inbox"; item.status = "open"; item.title = "MAZ answer";
                item.body = _reply; item.source = result.provider; item.ref = _takePath;
                store::addRecord(item);
                if (!result.reminderTitle.empty() && result.reminderDelay) {
                    store::Record reminder;
                    reminder.kind = "reminder"; reminder.status = "open";
                    reminder.title = result.reminderTitle; reminder.source = "voice";
                    scheduleReminder(reminder, result.reminderDelay);
                    store::addRecord(reminder);
                }
                store::remove(_takePath);
                _takePath.clear(); _haveTake = false;
                notify::post(Note::Success, "Answer ready", result.provider);
            } else {
                store::Record item;
                item.kind = "outbox"; item.status = "queued"; item.title = "Talk turn";
                item.body = result.error.empty() ? "host offline" : result.error;
                item.source = "talk"; item.ref = _takePath;
                store::addRecord(item);
                notify::post(Note::Warn, "Queued offline", "raw audio kept");
                _takePath.clear();
                _haveTake = false;
            }
            invalidate();
        }
        if (voice::state() == voice::State::Listening || voice::isPlaying())
            invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "MAZ Talk",
                   Sys.hostOnline ? "MAZ HOST ONLINE" : "MAZ HOST OFFLINE");

        if (!_reply.empty() && voice::state() != voice::State::Listening) {
            g.setFont(&fonts::Font0);
            g.setTextColor(ACCENT, BG);
            g.drawString(Cfg.talkRoute == 0 ? "LOCAL" : (Cfg.talkRoute == 2 ? "CLOUD" : "AUTO"), PAD, BODY_Y + 20);
            drawShortText(g, _reply, BODY_Y + 36);
            return;
        }

        char cap[48] = "";
        if (voice::state() == voice::State::Listening)
            snprintf(cap, sizeof(cap), "%s",
                     ui::hhmmss(voice::elapsedSeconds()).c_str());
        else if (_sending)
            snprintf(cap, sizeof(cap), "sending to laptop...");
        else if (!_reply.empty())
            snprintf(cap, sizeof(cap), "%s", ui::ellipsis(_reply, 34).c_str());
        else if (_haveTake)
            snprintf(cap, sizeof(cap), "take ready - ENTER sends");
        else
            snprintf(cap, sizeof(cap), "hold SPACE and speak");

        drawVoiceFace(g, voice::state(), cap);
    }

private:
    void beginTake() {
        discardTake();
        _sink = new voice::WavFileSink("outbox");
        if (!voice::start(_sink, MAX_SECONDS)) {
            notify::post(Note::Error, "Cannot record", voice::lastError());
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
        _takePath    = _sink->path();
        _takeSeconds = _sink->samples() / voice::SAMPLE_RATE;
        _haveTake    = true;
        delete _sink;
        _sink = nullptr;

        _sending = true;
        _sendAt = millis() + 180;  // paint SENDING before the blocking LAN turn
        invalidate();
    }

    void keepTake() {
        if (!_haveTake) return;
        const std::string dest = store::newPath("recordings", "wav");
        if (store::rename(_takePath, dest)) {
            notify::post(Note::Success, "Saved", "in Recorder");
            _haveTake = false;
            _takePath.clear();
        } else {
            notify::post(Note::Error, "Save failed",
                         "storage did not accept it");
        }
        invalidate();
    }

    void discardTake() {
        if (!_takePath.empty()) store::remove(_takePath);
        _takePath.clear();
        _haveTake = false;
    }

    static constexpr uint32_t MAX_SECONDS = 60;  // a turn, not a lecture

    voice::WavFileSink* _sink = nullptr;
    std::string         _takePath;
    uint32_t            _takeSeconds = 0;
    bool                _haveTake    = false;
    bool                _sending     = false;
    uint32_t            _sendAt      = 0;
    std::string         _session;
    std::string         _reply;
};

// ---------------------------------------------------------------- Capture
class CaptureApp : public App {
public:
    const char* id() const override { return "braindump"; }
    const char* title() const override { return "BrainDump"; }

    const char* hints() const override {
        if (voice::state() == voice::State::Listening)
            return "H highlight   P pause   ENTER finish";
        if (voice::state() == voice::State::Paused) return "P resume   ENTER finish";
        if (_processing) return "processing on laptop...";
        if (_ready) return "O process on laptop   P play raw";
        return "recording saved";
    }

    void onEnter() override {
        _highlights.clear();
        _ready = false;
        beginVoice();
        invalidate();
    }

    void onExit() override {
        if (voice::state() == voice::State::Listening || voice::state() == voice::State::Paused) endVoice();
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_H && voice::state() == voice::State::Listening) {
            _highlights.push_back(voice::elapsedSeconds());
            notify::post(Note::Success, "Highlighted", ui::hhmmss(voice::elapsedSeconds()));
            return true;
        }
        if (e.code == KEY_P && voice::state() == voice::State::Listening) { voice::pause(); invalidate(); return true; }
        if (e.code == KEY_P && voice::state() == voice::State::Paused) { voice::resume(); invalidate(); return true; }
        if (e.code == KEY_ENTER && (voice::state() == voice::State::Listening || voice::state() == voice::State::Paused)) { endVoice(); return true; }
        if (e.code == KEY_P && _ready) { voice::play(_path); return true; }
        if (e.code == KEY_O && _ready) {
            _processing = true; _processAt = millis() + 180; invalidate(); return true;
        }
        return false;
    }

    void update() override {
        if (_processing && millis() >= _processAt) process();
        if (voice::state() == voice::State::Listening) invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "BrainDump", store::backendName());

        if (voice::state() == voice::State::Listening) {
            drawVoiceFace(g, voice::state(),
                          ui::hhmmss(voice::elapsedSeconds()).c_str());
            return;
        }

        ui::emptyState(g, _processing ? "Processing on laptop" : (_ready ? "Raw thought preserved" : "Ready"),
                       _processing ? "raw audio remains on device" : (_ready ? "O sends a copy to the laptop" : "recording starts immediately"));
    }

private:
    void beginVoice() {
        _sink = new voice::WavFileSink("braindumps");
        if (!voice::start(_sink, 120)) {
            notify::post(Note::Error, "Cannot record", voice::lastError());
            delete _sink;
            _sink = nullptr;
            return;
        }
        sfx::recStart();
        invalidate();
    }

    void endVoice() {
        const bool   ok    = voice::stop();
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
        invalidate();
    }

    void process() {
        _processing = false;
        const auto result = host::brainDump(_path, _highlights);
        store::Record item;
        item.kind = result.ok ? "inbox" : "outbox";
        item.status = result.ok ? "open" : "queued";
        item.title = result.ok ? "BrainDump processed" : "BrainDump queued";
        item.body = result.ok ? result.text : result.error;
        item.source = "braindump"; item.ref = _path;
        store::addRecord(item);
        notify::post(result.ok ? Note::Success : Note::Warn,
                     result.ok ? "Useful output ready" : "Queued offline",
                     result.ok ? result.provider : "raw audio kept");
        invalidate();
    }

    voice::WavFileSink* _sink = nullptr;
    std::string         _path;
    std::vector<uint32_t> _highlights;
    bool                _ready = false;
    bool                _processing = false;
    uint32_t            _processAt = 0;
};

// --------------------------------------------------------------- Recorder
class RecorderApp : public App {
public:
    const char* id() const override { return "recorder"; }
    const char* title() const override { return "Recorder"; }

    const char* hints() const override {
        if (voice::state() == voice::State::Listening) return "ENTER stop & save";
        if (_files.empty()) return "ENTER record   ESC back";
        return "ENTER record   P play   D delete";
    }

    void onEnter() override {
        reload();
        invalidate();
    }
    void onExit() override {
        voice::stopPlayback();
        if (voice::state() == voice::State::Listening) stopRecording();
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;

        if (e.code == KEY_ENTER) {
            if (voice::state() == voice::State::Listening) stopRecording();
            else startRecording();
            return true;
        }
        if (voice::state() == voice::State::Listening) return false;

        if (_cursor.onKey(e, static_cast<int>(_files.size()))) {
            invalidate();
            return true;
        }
        if (_files.empty()) return false;

        if (e.code == KEY_P) {
            if (voice::isPlaying()) voice::stopPlayback();
            else voice::play(_files[_cursor.sel].path);
            invalidate();
            return true;
        }
        if (e.code == KEY_D) {
            // Two-step delete: the second D within the confirm window is the
            // one that acts. No modal, no accidental loss.
            if (_confirmDelete && millis() - _confirmAt < 3000) {
                store::remove(_files[_cursor.sel].path);
                notify::post(Note::Info, "Deleted", _files[_cursor.sel].name);
                _confirmDelete = false;
                reload();
            } else {
                _confirmDelete = true;
                _confirmAt     = millis();
                notify::post(Note::Warn, "Press D again", "to delete");
            }
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (voice::state() == voice::State::Listening || voice::isPlaying())
            invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);

        if (voice::state() == voice::State::Listening) {
            ui::header(g, "Recording", store::backendName());
            drawVoiceFace(g, voice::state(),
                          ui::hhmmss(voice::elapsedSeconds()).c_str());
            return;
        }

        char right[24];
        snprintf(right, sizeof(right), "%u files", (unsigned)_files.size());
        ui::header(g, "Recorder", right);

        if (_files.empty()) {
            ui::emptyState(g, "No recordings", "ENTER to start one");
            return;
        }

        const int rows = std::min<int>(theme::ROWS_VISIBLE - 1,
                                       static_cast<int>(_files.size()));
        for (int i = 0; i < rows; ++i) {
            const int idx = _cursor.first + i;
            if (idx >= static_cast<int>(_files.size())) break;
            ui::listRow(g, i + 1, idx == _cursor.sel,
                        ui::ellipsis(_files[idx].name, 20).c_str(),
                        ui::humanSize(_files[idx].size).c_str());
        }
        ui::scrollBar(g, static_cast<int>(_files.size()), _cursor.first,
                      theme::ROWS_VISIBLE - 1);
    }

private:
    void reload() {
        _files = store::list("recordings", ".wav");
        _cursor.clamp(static_cast<int>(_files.size()));
    }

    void startRecording() {
        if (!store::ready()) {
            notify::post(Note::Error, "No storage", "insert an SD card");
            return;
        }
        _sink = new voice::WavFileSink("recordings");
        if (!voice::start(_sink, MAX_SECONDS)) {
            notify::post(Note::Error, "Cannot record", voice::lastError());
            delete _sink;
            _sink = nullptr;
            return;
        }
        sfx::recStart();
        invalidate();
    }

    void stopRecording() {
        const bool   ok    = voice::stop();
        const size_t bytes = _sink ? _sink->samples() * 2 : 0;
        sfx::recStop();
        if (ok)
            notify::post(Note::Success, "Recording saved", ui::humanSize(bytes));
        else
            notify::post(Note::Error, "Recording failed", voice::lastError());
        delete _sink;
        _sink = nullptr;
        reload();
        invalidate();
    }

    // 30 minutes at 16kHz mono is about 57MB: fine on SD, and the cap stops a
    // forgotten recording from filling the card overnight.
    static constexpr uint32_t MAX_SECONDS = 30 * 60;

    std::vector<store::Entry> _files;
    ListCursor                _cursor;
    voice::WavFileSink*       _sink          = nullptr;
    bool                      _confirmDelete = false;
    uint32_t                  _confirmAt     = 0;
};

}  // namespace

App* makeCall() { return new CallApp(); }
App* makeCapture() { return new CaptureApp(); }
App* makeRecorder() { return new RecorderApp(); }

}  // namespace apps
}  // namespace maz
