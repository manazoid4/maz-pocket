#include "voice.h"

#include <FS.h>
#include <M5Unified.h>
#include <math.h>
#include <string.h>

#include <vector>

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
uint32_t    gPauseMs  = 0;
uint32_t    gPausedMs = 0;
float       gLevel    = 0.f;
bool        gClipped  = false;
uint32_t    gClipAtMs = 0;
const char* gErr      = "";

// Playback streaming state: recordings are far bigger than free SRAM, so we
// keep a file handle open and hand the speaker one block at a time.
// Two blocks are queued at once; each is ~128 ms so a slow SD read or a brief
// HTTP stall in the main loop does not starve the DMA. A block is only
// refilled once the speaker reports it is no longer queued (playRaw keeps the
// pointer, it does not copy), so a queued buffer is never overwritten.
constexpr size_t  kPlayBlock  = 2048;  // samples per queued block
constexpr uint8_t kPlayCh     = 0;     // dedicated channel: sfx tones use auto channels
constexpr uint8_t kMinVolume  = 64;    // floor for a reply; tiny values are inaudible
File     gPlayFile;
bool     gPlaying = false;
int16_t  gPlayBuf[2][kPlayBlock];
bool     gPlayEof = false;
size_t   gPlayBytes = 0;
size_t   gPlayRemain = 0;    // bytes of PCM data left in the data chunk
uint32_t gPlayRate = SAMPLE_RATE;
bool     gPlayStereo = false;
uint32_t gPlayStartMs = 0;
uint32_t gPlayDeadlineMs = 0;
uint8_t  gPlayIdx = 0;
uint32_t gPlayBlocks = 0;
std::vector<std::string> gPending;  // wavs queued behind the current one (max kMaxPending)
bool     gHold = false;             // keep playing at EOF: another part is still downloading
uint32_t gHoldUntil = 0;
constexpr size_t   kMaxPending = 4;
constexpr uint32_t kHoldMs = 25000;

// Generated audio (speaker self-test) goes through the exact same queue.
bool     gGen = false;
uint32_t gGenPos = 0;
float    gGenPhase = 0.f;
constexpr uint32_t kToneSamples  = SAMPLE_RATE;           // 1.0 s of 440 Hz
constexpr uint32_t kGapSamples   = SAMPLE_RATE / 10;      // 0.1 s silence
constexpr uint32_t kSweepSamples = SAMPLE_RATE * 8 / 10;  // 0.8 s sweep 300..2400 Hz
constexpr uint32_t kGenTotal     = kToneSamples + kGapSamples + kSweepSamples;

size_t genBlock(int16_t* out) {
    size_t n = 0;
    while (n < kPlayBlock && gGenPos < kGenTotal) {
        float hz = 0.f;
        if (gGenPos < kToneSamples) {
            hz = 440.f;
        } else if (gGenPos >= kToneSamples + kGapSamples) {
            const float t = (gGenPos - kToneSamples - kGapSamples) / (float)kSweepSamples;
            hz = 300.f + 2100.f * t;
        }
        gGenPhase += 2.f * (float)M_PI * hz / SAMPLE_RATE;
        if (gGenPhase > 2.f * (float)M_PI) gGenPhase -= 2.f * (float)M_PI;
        out[n++] = hz > 0.f ? (int16_t)(20000.f * sinf(gGenPhase)) : 0;
        ++gGenPos;
    }
    return n;
}

// Mic and speaker share one ES8311 codec on the ADV: Mic.end() powers the
// whole codec down (reg 0x00=0), and only Speaker.begin() runs the DAC
// power-up sequence. So: mic off first, then a clean speaker restart (end() is
// what makes begin() re-run the sequence if a stale task was still "running").
void prepareSpeaker() {
    M5.Mic.end();
    M5.Speaker.end();
    const bool ok = M5.Speaker.begin();
    uint8_t vol = Cfg.volume;
    if (vol < kMinVolume) {
        Serial.printf("[voice] volume %u too low, raising to %u for playback\n", vol, kMinVolume);
        vol = kMinVolume;
    }
    M5.Speaker.setVolume(vol);
    M5.Speaker.setChannelVolume(kPlayCh, 255);
    Serial.printf("[voice] speaker begin=%d enabled=%d vol=%u (cfg=%u)\n", ok,
                  M5.Speaker.isEnabled(), vol, Cfg.volume);
}

// Walks RIFF chunks to the real fmt/data offsets (Core's WAV may carry LIST or
// other chunks, so "data starts at 44" is not safe).
struct WavInfo {
    bool     ok = false;
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    uint32_t dataOff = 0, dataLen = 0;
};

WavInfo parseWav(File& f) {
    WavInfo w;
    const size_t fsz = f.size();
    uint8_t h[12];
    f.seek(0);
    if (f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) return w;
    size_t pos = 12;
    bool haveFmt = false;
    while (pos + 8 <= fsz) {
        uint8_t c[8];
        f.seek(pos);
        if (f.read(c, 8) != 8) break;
        const uint32_t len = c[4] | (c[5] << 8) | (c[6] << 16) | ((uint32_t)c[7] << 24);
        if (memcmp(c, "fmt ", 4) == 0 && len >= 16) {
            uint8_t b[16];
            if (f.read(b, 16) != 16) break;
            w.fmt = b[0] | (b[1] << 8);
            w.ch = b[2] | (b[3] << 8);
            w.rate = b[4] | (b[5] << 8) | (b[6] << 16) | ((uint32_t)b[7] << 24);
            w.bits = b[14] | (b[15] << 8);
            haveFmt = true;
        } else if (memcmp(c, "data", 4) == 0) {
            w.dataOff = pos + 8;
            const size_t avail = fsz > w.dataOff ? fsz - w.dataOff : 0;
            // Streamed WAVs may carry 0 / 0xFFFFFFFF as the size: trust the file.
            w.dataLen = (len == 0 || len > avail) ? avail : len;
            w.ok = haveFmt;
            return w;
        }
        pos += 8 + (size_t)len + (len & 1);
    }
    return w;
}

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
// Reads up to one block of PCM from the open file.
size_t readBlock(int16_t* out) {
    if (!gPlayFile) return 0;
    size_t want = kPlayBlock * sizeof(int16_t);
    if (want > gPlayRemain) want = gPlayRemain;
    want &= ~(size_t)1;
    size_t n = 0;
    if (want) n = gPlayFile.read(reinterpret_cast<uint8_t*>(out), want) / sizeof(int16_t);
    gPlayRemain = gPlayRemain > n * 2 ? gPlayRemain - n * 2 : 0;
    return n;
}

// Opens and validates a WAV for the queue; sets file, remaining bytes, rate, channels.
bool openWav(const std::string& wavPath, uint32_t* durationMs) {
    if (gPlayFile) gPlayFile.close();
    gPlayFile = store::fs()->open(wavPath.c_str(), FILE_READ);
    if (!gPlayFile) {
        gErr = "cannot open recording";
        Serial.printf("[voice] play open FAIL path=%s\n", wavPath.c_str());
        return false;
    }
    const WavInfo w = parseWav(gPlayFile);
    Serial.printf("[voice] play path=%s size=%u ok=%d fmt=%u ch=%u bits=%u rate=%lu dataOff=%u dataLen=%u vol=%u\n",
                  wavPath.c_str(), (unsigned)gPlayFile.size(), w.ok, w.fmt, w.ch, w.bits,
                  (unsigned long)w.rate, (unsigned)w.dataOff, (unsigned)w.dataLen, Cfg.volume);
    if (!w.ok || w.fmt != 1 || w.bits != 16 || (w.ch != 1 && w.ch != 2) || w.rate < 8000 ||
        w.rate > 48000 || w.dataLen < 2) {
        gErr = "unsupported wav";
        Serial.println("[voice] play FAIL: not 16-bit PCM mono/stereo wav");
        gPlayFile.close();
        return false;
    }
    if (w.rate != SAMPLE_RATE)
        Serial.printf("[voice] note: wav rate %lu != %lu, speaker resamples\n", (unsigned long)w.rate,
                      (unsigned long)SAMPLE_RATE);
    gPlayFile.seek(w.dataOff);
    gGen = false;
    gPlayRemain = w.dataLen;
    gPlayRate = w.rate;
    gPlayStereo = w.ch == 2;
    *durationMs = (uint32_t)((uint64_t)w.dataLen * 1000 / ((uint64_t)w.rate * w.ch * 2));
    return true;
}

// Moves to the next queued part (unreadable ones are skipped).
bool openNext() {
    while (!gPending.empty()) {
        const std::string next = gPending.front();
        gPending.erase(gPending.begin());
        uint32_t ms = 0;
        if (!openWav(next, &ms)) continue;
        gPlayEof = false;
        const uint32_t until = millis() + ms + 3000 + (gHold ? kHoldMs : 0);
        if ((int32_t)(until - gPlayDeadlineMs) > 0) gPlayDeadlineMs = until;
        return true;
    }
    return false;
}

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
        M5.Speaker.begin();  // do not leave the codec dead for UI beeps
        M5.Speaker.setVolume(Cfg.volume);
        gState = State::Error;
        return false;
    }

    gIdx          = 0;
    gHaveQueued   = false;
    gLevel        = 0.f;
    gClipped      = false;
    gStartMs      = millis();
    gMaxSec       = maxSeconds;
    gPauseMs      = 0;
    gPausedMs     = 0;
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
        // isPlaying(ch) is the number of blocks queued on that channel (0..2);
        // isPlaying() with no argument is a bool. Only refill a slot once the
        // speaker has released it, because playRaw keeps our pointer.
        if (!gPlayEof && M5.Speaker.isPlaying(kPlayCh) < 2) {
            size_t n = 0;
            if (gGen) {
                n = genBlock(gPlayBuf[gPlayIdx]);
            } else {
                n = readBlock(gPlayBuf[gPlayIdx]);
                // End of this part: roll straight into the next queued one so there is no gap.
                if (n == 0 && !gPending.empty() && openNext()) n = readBlock(gPlayBuf[gPlayIdx]);
            }
            if (n == 0) {
                gPlayEof = true;
            } else {
                gPlayBytes += n * 2;
                if (!M5.Speaker.playRaw(gPlayBuf[gPlayIdx], n, gPlayRate, gPlayStereo, 1, kPlayCh, false)) {
                    Serial.printf("[voice] playRaw REFUSED block=%u q=%u\n", (unsigned)gPlayBlocks,
                                  (unsigned)M5.Speaker.isPlaying(kPlayCh));
                    gPlayEof = true;
                } else {
                    if (gPlayBlocks == 0) Serial.printf("[voice] first block queued n=%u\n", (unsigned)n);
                    ++gPlayBlocks;
                    gPlayIdx = 1 - gPlayIdx;
                }
            }
        }
        // Let the queued tail finish before closing, or the last words are cut.
        if (gPlayEof && !gPending.empty() && openNext()) gPlayEof = false;  // part arrived after EOF
        if (gHold && gPlayEof && (int32_t)(millis() - gHoldUntil) > 0) gHold = false;  // never wait forever
        const bool drained = gPlayEof && !gHold && M5.Speaker.isPlaying(kPlayCh) == 0;
        if (drained || (int32_t)(millis() - gPlayDeadlineMs) > 0) {
            Serial.printf("[voice] play end bytes=%u blocks=%u ms=%u%s\n", (unsigned)gPlayBytes,
                          (unsigned)gPlayBlocks, (unsigned)(millis() - gPlayStartMs),
                          drained ? "" : " (deadline)");
            stopPlayback();
        }
    }
}

bool stop() {
    if (gState != State::Listening && gState != State::Paused) return false;
    gState = State::Saving;

    // Drain the block still in flight so the last words are not clipped off.
    if (gHaveQueued && gPauseMs == 0) {
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

bool pause() {
    if (gState != State::Listening) return false;
    if (gHaveQueued) {
        measure(gBuf[1 - gIdx], BLOCK);
        if (!gSink->write(gBuf[1 - gIdx], BLOCK)) {
            stop();
            gState = State::Error;
            return false;
        }
    }
    M5.Mic.end();
    gHaveQueued = false;
    gPauseMs    = millis();
    gState      = State::Paused;
    return true;
}

bool resume() {
    if (gState != State::Paused) return false;
    M5.Speaker.end();  // a tone while paused may have restarted it
    if (!M5.Mic.begin()) {
        // Close the still-open WAV cleanly; a failed codec restart must not
        // strand a file handle or leave the UI claiming it is recording.
        stop();
        gErr   = "mic did not resume";
        gState = State::Error;
        return false;
    }
    gPausedMs += millis() - gPauseMs;
    gPauseMs   = 0;
    gState     = State::Listening;
    return true;
}

State       state() { return gState; }
float       level() { return gLevel; }
bool        clipped() { return gClipped; }
const char* lastError() { return gErr; }

uint32_t elapsedSeconds() {
    if (gState != State::Listening && gState != State::Paused) return 0;
    const uint32_t paused = gPausedMs + (gPauseMs ? millis() - gPauseMs : 0);
    return (millis() - gStartMs - paused) / 1000;
}

// ----------------------------------------------------------------- playback
static void beginQueue(uint32_t rate, bool stereo, uint32_t durationMs) {
    prepareSpeaker();
    gPlayEof = false;
    gPlayBytes = 0;
    gPlayBlocks = 0;
    gPlayRate = rate;
    gPlayStereo = stereo;
    gPlayStartMs = millis();
    gPlayDeadlineMs = gPlayStartMs + durationMs + 3000;
    gPlaying = true;
    gState = State::Playing;
    gPlayIdx = 0;
}

bool play(const std::string& wavPath) {
    if (!store::ready()) return false;
    if (gState == State::Listening || gState == State::Paused) {
        Serial.println("[voice] play refused: recording in progress");
        return false;
    }
    stopPlayback();

    uint32_t ms = 0;
    if (!openWav(wavPath, &ms)) return false;
    beginQueue(gPlayRate, gPlayStereo, ms);
    return true;
}

bool enqueue(const std::string& wavPath) {
    if (!store::ready() || wavPath.empty()) return false;
    if (!gPlaying || gGen) return play(wavPath);
    if (gPending.size() >= kMaxPending) return false;
    gPending.push_back(wavPath);
    return true;
}

void holdOpen(bool on) {
    gHold = on && gPlaying;
    if (gHold) {
        gHoldUntil = millis() + kHoldMs;
        gPlayDeadlineMs = gHoldUntil + 3000;
    }
}

bool speakerTest(bool wait) {
    if (gState == State::Listening || gState == State::Paused) return false;
    stopPlayback();
    gGen = true;
    gGenPos = 0;
    gGenPhase = 0.f;
    beginQueue(SAMPLE_RATE, false, kGenTotal * 1000 / SAMPLE_RATE);
    if (wait) {
        while (gPlaying) {
            update();
            delay(5);
        }
        Serial.printf("[spk] test done (blocks=%u vol=%u)\n", (unsigned)gPlayBlocks, (unsigned)Cfg.volume);
    }
    return true;
}

void stopPlayback() {
    if (!gPlaying) return;
    M5.Speaker.stop(kPlayCh);  // only our channel; a UI beep is not ours to cut
    if (gPlayFile) gPlayFile.close();
    gPlaying = false;
    gGen = false;
    gPending.clear();
    gHold = false;
    if (gState == State::Playing) gState = State::Idle;
}

bool isPlaying() { return gPlaying; }

uint32_t wavSeconds(const std::string& wavPath) {
    if (!store::ready()) return 0;
    File f = store::fs()->open(wavPath.c_str(), FILE_READ);
    if (!f) return 0;
    const WavInfo w = parseWav(f);
    f.close();
    return w.ok && w.rate ? w.dataLen / (w.rate * w.ch * 2) : 0;
}

}  // namespace voice
}  // namespace maz
