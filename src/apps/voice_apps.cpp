// Long-form Recorder. COMM and CAPTURE live in their dedicated current files.
#include <algorithm>
#include <vector>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/notify.h"
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
        case voice::State::Paused: return "PAUSED";
        case voice::State::Saving: return "SAVING";
        case voice::State::Playing: return "PLAYING";
        case voice::State::Error: return "ERROR";
        default: return "READY";
    }
}

uint16_t stateColour(voice::State s) {
    switch (s) {
        case voice::State::Listening: return ACCENT2;
        case voice::State::Playing: return OK;
        case voice::State::Error: return ERR;
        default: return DIM;
    }
}

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

class RecorderApp final : public App {
public:
    const char* id() const override { return "recorder"; }
    const char* title() const override { return "Recorder"; }

    const char* hints() const override {
        if (voice::state() == voice::State::Listening) return "ENTER stop & save";
        if (_files.empty()) return "ENTER record   ESC back";
        return "ENTER record   P play   D delete";
    }

    void onEnter() override {
        voice::setCaptureTap(nullptr);
        reload();
        invalidate();
    }

    void onExit() override {
        voice::setCaptureTap(nullptr);
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
            if (_confirmDelete && millis() - _confirmAt < 3000) {
                store::remove(_files[_cursor.sel].path);
                notify::post(Note::Info, "Deleted", _files[_cursor.sel].name);
                _confirmDelete = false;
                reload();
            } else {
                _confirmDelete = true;
                _confirmAt = millis();
                notify::post(Note::Warn, "Press D again", "to delete");
            }
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (voice::state() == voice::State::Listening || voice::isPlaying()) invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        if (voice::state() == voice::State::Listening) {
            ui::header(g, "Recording", store::backendName());
            drawVoiceFace(g, voice::state(), ui::hhmmss(voice::elapsedSeconds()).c_str());
            return;
        }
        char right[24];
        snprintf(right, sizeof(right), "%u files", static_cast<unsigned>(_files.size()));
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
        voice::setCaptureTap(nullptr);
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
        const bool ok = voice::stop();
        const size_t bytes = _sink ? _sink->samples() * 2 : 0;
        sfx::recStop();
        if (ok) notify::post(Note::Success, "Recording saved", ui::humanSize(bytes));
        else notify::post(Note::Error, "Recording failed", voice::lastError());
        delete _sink;
        _sink = nullptr;
        reload();
        invalidate();
    }

    static constexpr uint32_t MAX_SECONDS = 30 * 60;
    std::vector<store::Entry> _files;
    ListCursor _cursor;
    voice::WavFileSink* _sink = nullptr;
    bool _confirmDelete = false;
    uint32_t _confirmAt = 0;
};

}  // namespace

App* makeRecorder() { return new RecorderApp(); }

}  // namespace apps
}  // namespace maz
