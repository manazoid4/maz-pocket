// MAZ Pocket — the voice pipeline.
//
// This is the piece v0.2 replaces, so it is deliberately drawn as a pipeline
// and not as "the record button". Today:
//
//     mic -> VoiceSink(WavFile) -> speaker
//
// Next version, Call swaps the sink and nothing else changes:
//
//     mic -> VoiceSink(Net -> OpenFlowKit -> MAZos -> TTS) -> speaker
//
// Call/Capture/Recorder all talk to this module, so none of them know or care
// where the audio actually goes.
#pragma once
#include <stdint.h>

#include <string>

namespace maz {
namespace voice {

// 16kHz mono 16-bit: the format every speech model wants, small enough that
// an ESP32-S3 with no PSRAM can stream it to SD without dropping frames.
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t   BLOCK       = 512;  // samples per DMA block, about 32ms

enum class State : uint8_t { Idle, Listening, Paused, Saving, Playing, Error };

// Where captured audio goes. Implement this to add a destination.
class Sink {
public:
    virtual ~Sink()                                      = default;
    virtual bool        open()                           = 0;
    virtual bool        write(const int16_t* s, size_t n) = 0;
    virtual bool        close()                          = 0;
    virtual const char* name() const                     = 0;
};

// The v0.1 sink: a RIFF/WAVE file under /maz/<sub>/.
class WavFileSink : public Sink {
public:
    explicit WavFileSink(const char* subdir) : _sub(subdir) {}
    bool        open() override;
    bool        write(const int16_t* s, size_t n) override;
    bool        close() override;
    const char* name() const override { return "wav"; }

    const std::string& path() const { return _path; }
    uint32_t           samples() const { return _samples; }

private:
    const char* _sub;
    std::string _path;
    uint32_t    _samples = 0;
    void*       _file    = nullptr;
};

bool begin();  // one-time audio bring-up

// --- capture --------------------------------------------------------------
// maxSeconds guards against a pocket-pressed SPACE filling the card.
bool        start(Sink* sink, uint32_t maxSeconds);
void        update();  // pump: call every loop while listening or playing
bool        stop();
bool        pause();
bool        resume();
State       state();
float       level();  // 0..1 smoothed peak, drives the listening animation
uint32_t    elapsedSeconds();
bool        clipped();  // true if the last second hit full scale
const char* lastError();

// --- playback -------------------------------------------------------------
bool play(const std::string& wavPath);
void stopPlayback();
bool isPlaying();

// Duration of a MAZ-written wav without loading it, for list screens.
uint32_t wavSeconds(const std::string& wavPath);

}  // namespace voice
}  // namespace maz
