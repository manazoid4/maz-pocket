// Call, Capture and Recorder — the three voice surfaces.
//
// They share one recording engine (audio/voice) and differ only in intent:
//   Call     — a conversation turn. Short cap, review then keep or bin.
//              This is the screen v0.2 rewires to OpenFlowKit.
//   Capture  — a thought. Fastest path from pocket to stored, voice or text.
//   Recorder — a session. Long-form, file management, playback.
#include <algorithm>
#include <vector>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/notify.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
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

// ------------------------------------------------------------------- Call
class CallApp : public App {
public:
    const char* id() const override { return "call"; }
    const char* title() const override { return "Call"; }

    const char* hints() const override {
        if (voice::state() == voice::State::Listening)
            return "release SPACE to stop";
        if (_haveTake) return "P play   S save   D delete   SPACE again";
        return "hold SPACE to talk   ESC back";
    }

    void onEnter() override {
        _haveTake = false;
        _sink     = nullptr;
        invalidate();
    }

    void onExit() override {
        voice::stopPlayback();
        if (voice::state() == voice::State::Listening) voice::stop();
        discardTake();
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

        if (_haveTake && e.code == KEY_P) {
            voice::play(_takePath);
            invalidate();
            return true;
        }
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
        if (voice::state() == voice::State::Listening || voice::isPlaying())
            invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "Call",
                   Sys.hostOnline ? "MAZ HOST ONLINE" : "MAZ HOST OFFLINE");

        char cap[48] = "";
        if (voice::state() == voice::State::Listening)
            snprintf(cap, sizeof(cap), "%s",
                     ui::hhmmss(voice::elapsedSeconds()).c_str());
        else if (_haveTake)
            snprintf(cap, sizeof(cap), "take ready - %us", (unsigned)_takeSeconds);
        else
            snprintf(cap, sizeof(cap), "local only in v0.1");

        drawVoiceFace(g, voice::state(), cap);
    }

private:
    void beginTake() {
        discardTake();
        _sink = new voice::WavFileSink("cache");
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

        // Auto-play the take: this is the closest v0.1 gets to a reply, and it
        // is also the fastest way to hear whether the mic is behaving.
        voice::play(_takePath);
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
};

// ---------------------------------------------------------------- Capture
class CaptureApp : public App {
public:
    const char* id() const override { return "capture"; }
    const char* title() const override { return "Capture"; }

    const char* hints() const override {
        if (voice::state() == voice::State::Listening)
            return "release SPACE to save";
        if (!_field.text.empty()) return "ENTER save note   ESC discard";
        return "hold SPACE voice   or just type";
    }

    void onEnter() override {
        _field.text.clear();
        // Entered by holding SPACE from Home: start immediately rather than
        // making the user press it again for the same gesture.
        if (KB.held(KEY_SPACE)) beginVoice();
        invalidate();
    }

    void onExit() override {
        if (voice::state() == voice::State::Listening) endVoice();
    }

    bool onKey(const KeyEvent& e) override {
        if (e.down && e.code == KEY_SPACE && _field.text.empty()) {
            if (voice::state() != voice::State::Listening) beginVoice();
            return true;
        }
        if (!e.down && e.code == KEY_SPACE) {
            if (voice::state() == voice::State::Listening) endVoice();
            return true;
        }
        if (!e.down) return false;

        if (e.code == KEY_ENTER) {
            saveText();
            return true;
        }
        if (_field.onKey(e)) {
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (voice::state() == voice::State::Listening) invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "Capture", store::backendName());

        if (voice::state() == voice::State::Listening) {
            drawVoiceFace(g, voice::state(),
                          ui::hhmmss(voice::elapsedSeconds()).c_str());
            return;
        }

        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.setTextDatum(top_left);
        g.drawString("thought in, nothing else", PAD, BODY_Y + 26);
        _field.draw(g, PAD, BODY_Y + 40, SCREEN_W - PAD * 2, "type a note...");

        g.setTextColor(DIM, BG);
        g.drawString("hold SPACE for a voice memo instead", PAD, BODY_Y + 66);
    }

private:
    void beginVoice() {
        _sink = new voice::WavFileSink("captures");
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
        sfx::recStop();
        if (ok)
            notify::post(Note::Success, "Captured", ui::humanSize(bytes));
        else
            notify::post(Note::Error, "Nothing captured", voice::lastError());
        delete _sink;
        _sink = nullptr;
        invalidate();
    }

    void saveText() {
        if (_field.text.empty()) return;
        const std::string path = store::newPath("captures", "txt");
        if (store::writeText(path, _field.text + "\n")) {
            notify::post(Note::Success, "Captured", "saved as note");
            _field.text.clear();
        } else {
            notify::post(Note::Error, "Save failed", store::backendName());
        }
        invalidate();
    }

    TextField           _field;
    voice::WavFileSink* _sink = nullptr;
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
