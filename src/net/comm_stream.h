#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace maz {
namespace comm_stream {

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint16_t FRAME_MS = 20;
constexpr size_t FRAME_SAMPLES = SAMPLE_RATE * FRAME_MS / 1000;  // 320
constexpr size_t FRAME_BYTES = FRAME_SAMPLES * sizeof(int16_t);   // 640

enum class State : uint8_t {
    Idle,
    Connecting,
    Streaming,
    Finishing,
    Waiting,
    Done,
    Failed,
    Cancelled,
};

enum class EventType : uint8_t {
    Ready,
    Transcript,
    Delta,
    Done,
    Error,
    Disconnected,
    Cancelled,
};

struct Event {
    EventType type = EventType::Error;
    std::string turnId;
    std::string sessionId;
    std::string text;
    std::string provider;
    uint32_t firstTokenMs = 0;
    uint32_t totalMs = 0;
};

struct Stats {
    uint32_t turns = 0;
    uint32_t framesQueued = 0;
    uint32_t framesSent = 0;
    uint32_t frameDrops = 0;
    uint32_t reconnects = 0;
    uint32_t protocolErrors = 0;
    uint32_t lastFirstTokenMs = 0;
    uint32_t lastTotalMs = 0;
    uint32_t stackMinWords = 0;
    uint8_t queueDepth = 0;
    uint8_t queueHighWater = 0;
};

bool begin();

// LAN WebSocket is the low-latency primary path. Remote HTTPS continues to use
// the proven REST fallback because raw audio is always kept on SD first.
bool start(const std::string& sessionId, const char* route);

// Called by the audio tap after the exact PCM was safely written to the WAV.
// It packetises arbitrary capture blocks into exact 20 ms network frames.
bool pushPcm(const int16_t* samples, size_t count);

// Flushes/pads the final partial frame and asks the worker to send END only
// after all queued audio frames have left the bounded queue.
void finish();
void cancel();
void reset();

bool poll(Event& out);
State state();
bool usable();       // stream is connected or still safely connecting
bool failed();       // caller should use the saved-WAV REST fallback
std::string sessionId();
std::string turnId();
Stats stats();

}  // namespace comm_stream
}  // namespace maz
