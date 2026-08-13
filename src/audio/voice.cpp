#include "voice.h"

#include <FS.h>
#include <M5Unified.h>

#include "../core/settings.h"
#include "../core/sys.h"
#include "../storage/store.h"

namespace maz {
namespace voice {

namespace {

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4]       = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize     = 0;
    char     wave[4]       = {'W', 'A', 'V', 'E'};
    char     fmt[4]        = {'f', 'm', 't', ' '};
    uint32_t fmtSize       = 16;
    uint16_t audioFormat   = 1;  // PCM
    uint16_t channels      = 1;
    uint32_t sampleRate    = SAMPLE_RATE;
    uint32_t byteRate      = SAMPLE_RATE * 2;
    uint16_t blockAlign    = 2;
    uint16_t bitsPerSample = 16;
    char     data[4]       = {'d', 'a', 't', 'a'};
    uint32_t dataSize      = 0;
};
#pragma pack(pop)

// Double-buffered so the I2S DMA always has somewhere to land while we are
// busy writing the other half to a card that may stall for milliseconds.
int16_t gBuf[2][BLOCK];
uint8_t gIdx        = 0;
bool    gHaveQueued = false;

State       gState    = State::Idle;
Sink*       gSink     = nullptr;
uint32_t    gStartMs  = 0;
uint32_t    gMaxSec   = 60;
float       gLevel    = 0.f;
bool        gClipped  = false;
uint32_t    gClipAtMs = 0;
const char* gErr      = "";

// Playback streaming state: recordings are far bigger than free SRAM, so we
// keep a file handle open and hand the speaker one block at a time.
File    gPlayFile;
bool    gPlaying = false;
int16_t gPlayBuf[2][BLOCK];
uint8_t gPlayIdx = 0;

void measure(const int16_t* s, size_t n) {
    int32_t peak = 0;
    for (size_t i = 0; i < n; ++i) {
        const int32_t v = s[i] < 0 ? -s[i] : s[i];
        if (v > peak) peak = v;
    }
    if (peak > 32000) {
        gClipped  = true;
        gClipAtMs = millis();
    } else if (millis() - gClipAtMs > 1000) {
        gClipped = false;
    }
    // Attack fast, release slow: the ring should jump when you speak and
    // settle gently, not flicker.
    const float target = peak / 32768.f;
    gLevel = target > gLevel ? target : gLevel * 0.85f + target * 0.15f;
}

}  // namespace

// -------------------------------------------------------------- WavFileSink
bool WavFileSink::open() {
    if (!store::ready()) {
        gErr = "no storage";
        return false;
    }
    _path    = store::newPath(_sub, "wav");
    _samples = 0;

    File* f = new File(store::fs()->open(_path.c_str(), FILE_WRITE));
    if (!*f) {
        delete f;
        gErr = "cannot create file";
        return false;
    }
    WavHeader h;  // sizes patched in close()
    f->write(reinterpret_cast<const uint8_t*>(&h), sizeof(h));
    _file = f;
    return true;
}

bool WavFileSink::write(const int16_t* s, size_t n) {
    if (!_file) return false;
    File*        f    = static_cast<File*>(_file);
    const size_t want = n * sizeof(int16_t);
    if (f->write(reinterpret_cast<const uint8_t*>(s), want) != want) {
        gErr = "write failed";
        return false;
    }
    _samples += n;
    return true;
}

bool WavFileSink::close() {
    if (!_file) return false;
    File* f = static_cast<File*>(_file);

    WavHeader h;
    h.dataSize  = _samples * 2;
    h.chunkSize = h.dataSize + sizeof(WavHeader) - 8;
    f->seek(0);
    f->write(reinterpret_cast<const uint8_t*>(&h), sizeof(h));
    f->close();
    delete f;
    _file = nullptr;
    return _samples > 0;
}

// -------------------------------------------------------------------- setup
bool begin() {
    // The ADV routes both directions through one ES8311 codec, so mic and
    // speaker cannot be live at the same time; we flip between them.
    auto mcfg          = M5.Mic.config();
    mcfg.sample_rate   = SAMPLE_RATE;
    mcfg.magnification = Cfg.micGain;  // gain lives in settings, user-tunable
    mcfg.over_sampling = 1;
    mcfg.dma_buf_len   = 256;
    mcfg.dma_buf_count = 8;
    M5.Mic.config(mcfg);

    auto scfg        = M5.Speaker.config();
    scfg.sample_rate = SAMPLE_RATE;
    M5.Speaker.config(scfg);
    M5.Speaker.setVolume(Cfg.volume);
    return true;
}

// ------------------------------------------------------------------ capture
bool start(Sink* sink, uint32_t maxSeconds) {
    if (gState == State::Listening) return false;
    stopPlayback();

    gSink = sink;
    if (!gSink || !gSink->open()) {
        gState = State::Error;
        return false;
    }

    M5.Speaker.end();
    if (!M5.Mic.begin()) {
        gErr = "mic did not start";
        gSink->close();
        gState = State::Error;
        return false;
    }

    gIdx          = 0;
    gHaveQueued   = false;
    gLevel        = 0.f;
    gClipped      = false;
    gStartMs      = millis();
    gMaxSec       = maxSeconds;
    gState        = State::Listening;
    Sys.recording = true;
    return true;
}

void update() {
    if (gState == State::Listening) {
        Sys.recSeconds = elapsedSeconds();
        if (elapsedSeconds() >= gMaxSec) {
            stop();
            return;
        }
        if (M5.Mic.record(gBuf[gIdx], BLOCK, SAMPLE_RATE)) {
            // record() returning true means this block is queued; the other
            // block has been filled and is ours to drain.
            gIdx = 1 - gIdx;
            if (gHaveQueued) {
                measure(gBuf[gIdx], BLOCK);
                if (!gSink->write(gBuf[gIdx], BLOCK)) {
                    stop();
                    gState = State::Error;
                    return;
                }
            }
            gHaveQueued = true;
        }
        return;
    }

    if (gPlaying) {
        // Keep at most one block queued so ESC stops playback promptly.
        if (M5.Speaker.isPlaying() < 2) {
            const size_t n =
                gPlayFile.read(reinterpret_cast<uint8_t*>(gPlayBuf[gPlayIdx]),
                               BLOCK * sizeof(int16_t)) /
                sizeof(int16_t);
            if (n == 0) {
                stopPlayback();
                return;
            }
            M5.Speaker.playRaw(gPlayBuf[gPlayIdx], n, SAMPLE_RATE, false, 1, -1);
            gPlayIdx = 1 - gPlayIdx;
        }
    }
}

bool stop() {
    if (gState != State::Listening) return false;
    gState = State::Saving;

    // Drain the block still in flight so the last words are not clipped off.
    if (gHaveQueued) {
        measure(gBuf[1 - gIdx], BLOCK);
        gSink->write(gBuf[1 - gIdx], BLOCK);
    }

    M5.Mic.end();
    const bool ok = gSink->close();
    M5.Speaker.begin();
    M5.Speaker.setVolume(Cfg.volume);

    Sys.recording  = false;
    Sys.recSeconds = 0;
    gState         = ok ? State::Idle : State::Error;
    if (!ok) gErr = "nothing recorded";
    return ok;
}

State       state() { return gState; }
float       level() { return gLevel; }
bool        clipped() { return gClipped; }
const char* lastError() { return gErr; }

uint32_t elapsedSeconds() {
    if (gState != State::Listening) return 0;
    return (millis() - gStartMs) / 1000;
}

// ----------------------------------------------------------------- playback
bool play(const std::string& wavPath) {
    if (!store::ready()) return false;
    stopPlayback();

    gPlayFile = store::fs()->open(wavPath.c_str(), FILE_READ);
    if (!gPlayFile) {
        gErr = "cannot open recording";
        return false;
    }
    gPlayFile.seek(sizeof(WavHeader));  // we only ever read our own headers
    M5.Speaker.begin();
    M5.Speaker.setVolume(Cfg.volume);
    gPlaying = true;
    gState   = State::Playing;
    gPlayIdx = 0;
    return true;
}

void stopPlayback() {
    if (!gPlaying) return;
    M5.Speaker.stop();
    gPlayFile.close();
    gPlaying = false;
    if (gState == State::Playing) gState = State::Idle;
}

bool isPlaying() { return gPlaying; }

uint32_t wavSeconds(const std::string& wavPath) {
    if (!store::ready()) return 0;
    File f = store::fs()->open(wavPath.c_str(), FILE_READ);
    if (!f) return 0;
    const size_t bytes =
        f.size() > sizeof(WavHeader) ? f.size() - sizeof(WavHeader) : 0;
    f.close();
    return bytes / (SAMPLE_RATE * 2);
}

}  // namespace voice
}  // namespace maz
