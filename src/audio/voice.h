// MAZ Pocket — the voice pipeline.
#pragma once
#include <stdint.h>

#include <string>

namespace maz {
namespace voice {

// 16kHz mono 16-bit is the one capture format used everywhere. The SD writer
// may use larger DMA blocks; COMM packetises the post-write tap into 20ms WS
// frames without changing the durable WAV format.
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t   BLOCK       = 512;

enum class State : uint8_t { Idle, Listening, Paused, Saving, Playing, Error };

class Sink {
public:
    virtual ~Sink()                                        = default;
    virtual bool        open()                             = 0;
    virtual bool        write(const int16_t* s, size_t n) = 0;
    virtual bool        close()                            = 0;
    virtual const char* name() const                       = 0;
};

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

using PcmTap = void (*)(const int16_t* samples, size_t count);

bool begin();

// Optional observer called only AFTER the exact PCM block was safely written
// to the current sink. COMM uses this to stream; the local WAV remains truth.
void setCaptureTap(PcmTap tap);

bool        start(Sink* sink, uint32_t maxSeconds);
void        update();
bool        stop();
bool        pause();
bool        resume();
State       state();
float       level();
uint32_t    elapsedSeconds();
bool        clipped();
const char* lastError();

bool play(const std::string& wavPath);
void stopPlayback();
bool isPlaying();
uint32_t wavSeconds(const std::string& wavPath);

}  // namespace voice
}  // namespace maz
