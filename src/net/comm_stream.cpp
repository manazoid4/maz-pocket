#include "comm_stream.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_websocket_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>

#include "../core/settings.h"
#include "mazhost.h"

namespace maz {
namespace comm_stream {
namespace {

constexpr uint8_t AUDIO_CAPACITY = 20;  // 400 ms; bounded, ~12.8 KB
constexpr uint8_t EVENT_CAPACITY = 16;
constexpr uint8_t START_CAPACITY = 1;
constexpr size_t RX_JSON_MAX = 4096;

struct PcmFrame {
    int16_t samples[FRAME_SAMPLES];
};

struct StartRequest {
    std::string session;
    std::string route;
    std::string turn;
};

QueueHandle_t gAudio = nullptr;
QueueHandle_t gEvents = nullptr;
QueueHandle_t gStarts = nullptr;
TaskHandle_t gTask = nullptr;
esp_websocket_client_handle_t gClient = nullptr;

volatile State gState = State::Idle;
volatile bool gConnected = false;
volatile bool gDisconnected = false;
volatile bool gIntentionalStop = false;
volatile bool gFinishRequested = false;
volatile bool gCancelRequested = false;
volatile bool gStartSent = false;
volatile bool gProtocolFailed = false;

std::string gActiveSession;
std::string gActiveTurn;
std::string gRoute = "local";
std::string gUri;
std::string gHeaders;
std::string gRxText;

PcmFrame gPartial{};
size_t gPartialCount = 0;
uint32_t gFramesThisTurn = 0;
uint8_t gReconnectAttempts = 0;

volatile uint32_t gTurns = 0;
volatile uint32_t gFramesQueued = 0;
volatile uint32_t gFramesSent = 0;
volatile uint32_t gFrameDrops = 0;
volatile uint32_t gReconnects = 0;
volatile uint32_t gProtocolErrors = 0;
volatile uint32_t gLastFirstToken = 0;
volatile uint32_t gLastTotal = 0;
volatile uint32_t gStackMinWords = 0;
volatile uint8_t gQueueHighWater = 0;

void publish(Event* event) {
    if (!event || !gEvents) {
        delete event;
        return;
    }
    if (xQueueSend(gEvents, &event, 0) == pdTRUE) return;

    // Delta events are presentation hints; DONE always contains the complete
    // reply. Make room for terminal events rather than ever blocking the WS
    // callback/task behind a slow display frame.
    Event* old = nullptr;
    if (xQueueReceive(gEvents, &old, 0) == pdTRUE) delete old;
    if (xQueueSend(gEvents, &event, 0) != pdTRUE) delete event;
}

void publishSimple(EventType type, const std::string& text = {}) {
    Event* event = new Event();
    if (!event) return;
    event->type = type;
    event->turnId = gActiveTurn;
    event->sessionId = gActiveSession;
    event->text = text;
    publish(event);
}

void parseServerJson(const char* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        ++gProtocolErrors;
        gProtocolFailed = true;
        publishSimple(EventType::Error, "invalid stream JSON");
        return;
    }
    const std::string type = doc["type"] | "";
    Event* event = new Event();
    if (!event) return;
    event->turnId = doc["turn_id"] | gActiveTurn.c_str();
    event->sessionId = doc["session_id"] | gActiveSession.c_str();

    if (type == "ready") {
        event->type = EventType::Ready;
        if (!event->sessionId.empty()) gActiveSession = event->sessionId;
        gState = State::Streaming;
    } else if (type == "transcript") {
        event->type = EventType::Transcript;
        event->text = doc["text"] | "";
    } else if (type == "delta") {
        event->type = EventType::Delta;
        event->text = doc["text"] | "";
    } else if (type == "done") {
        event->type = EventType::Done;
        event->text = doc["reply"] | "";
        event->provider = doc["provider"] | "";
        event->firstTokenMs = doc["timings"]["first_token_ms"] | 0;
        event->totalMs = doc["timings"]["total_ms"] | 0;
        gLastFirstToken = event->firstTokenMs;
        gLastTotal = event->totalMs;
        gState = State::Done;
    } else if (type == "error") {
        event->type = EventType::Error;
        event->text = doc["error"] | "stream failed";
        gState = State::Failed;
    } else if (type == "cancelled") {
        event->type = EventType::Cancelled;
        gState = State::Cancelled;
    } else if (type == "pong") {
        delete event;
        return;
    } else {
        delete event;
        ++gProtocolErrors;
        gProtocolFailed = true;
        publishSimple(EventType::Error, "unknown stream event");
        return;
    }
    publish(event);
}

void wsEvent(void*, esp_event_base_t, int32_t eventId, void* eventData) {
    auto* data = static_cast<esp_websocket_event_data_t*>(eventData);
    if (eventId == WEBSOCKET_EVENT_CONNECTED) {
        gConnected = true;
        gDisconnected = false;
        return;
    }
    if (eventId == WEBSOCKET_EVENT_DISCONNECTED || eventId == WEBSOCKET_EVENT_CLOSED) {
        gConnected = false;
        if (!gIntentionalStop) gDisconnected = true;
        return;
    }
    if (eventId == WEBSOCKET_EVENT_ERROR) {
        if (!gIntentionalStop) gDisconnected = true;
        return;
    }
    if (eventId != WEBSOCKET_EVENT_DATA || !data || data->op_code != 0x1) return;

    if (data->payload_len <= 0 || data->payload_len > static_cast<int>(RX_JSON_MAX)) {
        ++gProtocolErrors;
        gProtocolFailed = true;
        return;
    }
    if (data->payload_offset == 0) {
        gRxText.clear();
        gRxText.reserve(data->payload_len);
    }
    if (data->data_ptr && data->data_len > 0)
        gRxText.append(data->data_ptr, static_cast<size_t>(data->data_len));
    if (data->payload_offset + data->data_len >= data->payload_len)
        parseServerJson(gRxText.data(), gRxText.size());
}

void teardown() {
    if (!gClient) return;
    gIntentionalStop = true;
    esp_websocket_client_stop(gClient);
    esp_websocket_client_destroy(gClient);
    gClient = nullptr;
    gIntentionalStop = false;
    gConnected = false;
    gDisconnected = false;
    gStartSent = false;
}

bool openClient() {
    if (Cfg.hostAddr.empty() || Cfg.hostToken.empty() || WiFi.status() != WL_CONNECTED)
        return false;

    gUri = "ws://" + Cfg.hostAddr + ":" + std::to_string(Cfg.hostPort) + "/ws/comm";
    gHeaders = "Authorization: Bearer " + Cfg.hostToken + "\r\n";

    esp_websocket_client_config_t cfg{};
    cfg.uri = gUri.c_str();
    cfg.headers = gHeaders.c_str();
    cfg.disable_auto_reconnect = true;
    cfg.task_stack = 4096;
    cfg.buffer_size = 2048;
    cfg.ping_interval_sec = 10;
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = 5;
    cfg.keep_alive_interval = 5;
    cfg.keep_alive_count = 3;

    gClient = esp_websocket_client_init(&cfg);
    if (!gClient) return false;
    esp_websocket_register_events(gClient, WEBSOCKET_EVENT_ANY, wsEvent, nullptr);
    if (esp_websocket_client_start(gClient) != ESP_OK) {
        esp_websocket_client_destroy(gClient);
        gClient = nullptr;
        return false;
    }
    gConnected = false;
    gDisconnected = false;
    gStartSent = false;
    gState = State::Connecting;
    return true;
}

bool sendText(const String& text) {
    if (!gClient || !gConnected) return false;
    return esp_websocket_client_send_text(
               gClient, text.c_str(), static_cast<int>(text.length()),
               pdMS_TO_TICKS(150)) == static_cast<int>(text.length());
}

bool sendStart() {
    JsonDocument doc;
    doc["type"] = "start";
    doc["protocol"] = 1;
    doc["turn_id"] = gActiveTurn;
    if (!gActiveSession.empty()) doc["session_id"] = gActiveSession;
    doc["route"] = gRoute;
    JsonObject audio = doc["audio"].to<JsonObject>();
    audio["format"] = "pcm_s16le";
    audio["sample_rate"] = SAMPLE_RATE;
    audio["channels"] = 1;
    audio["frame_ms"] = FRAME_MS;
    String text;
    serializeJson(doc, text);
    return sendText(text);
}

bool sendEnd() {
    JsonDocument doc;
    doc["type"] = "end";
    doc["turn_id"] = gActiveTurn;
    String text;
    serializeJson(doc, text);
    return sendText(text);
}

void sendCancel() {
    if (!gClient || !gConnected) return;
    JsonDocument doc;
    doc["type"] = "cancel";
    doc["turn_id"] = gActiveTurn;
    String text;
    serializeJson(doc, text);
    sendText(text);
}

void fail(const char* message, bool disconnected = false) {
    gState = State::Failed;
    if (disconnected) publishSimple(EventType::Disconnected, message);
    else publishSimple(EventType::Error, message);
}

void worker(void*) {
    for (;;) {
        StartRequest* request = nullptr;
        if (xQueueReceive(gStarts, &request, 0) == pdTRUE && request) {
            teardown();
            xQueueReset(gAudio);
            gActiveSession = request->session;
            gRoute = request->route;
            gActiveTurn = request->turn;
            gFramesThisTurn = 0;
            gReconnectAttempts = 0;
            gFinishRequested = false;
            gCancelRequested = false;
            gProtocolFailed = false;
            if (!openClient()) fail("stream connect unavailable");
            delete request;
        }

        if (gCancelRequested) {
            sendCancel();
            teardown();
            gCancelRequested = false;
            gState = State::Cancelled;
        }

        if (gProtocolFailed) {
            teardown();
            gProtocolFailed = false;
            gState = State::Failed;
        }

        if (gDisconnected) {
            gDisconnected = false;
            // Reconnect only before any audio has left the device. Resuming a
            // half-sent turn would silently lose speech; once audio was sent we
            // prefer the complete SD WAV + REST fallback.
            if (gFramesThisTurn == 0 && gReconnectAttempts < 1 && !gFinishRequested) {
                ++gReconnectAttempts;
                ++gReconnects;
                teardown();
                vTaskDelay(pdMS_TO_TICKS(150));
                if (!openClient()) fail("stream reconnect failed", true);
            } else {
                teardown();
                fail("stream disconnected; using REST", true);
            }
        }

        if (gClient && gConnected && !gStartSent) {
            if (sendStart()) {
                gStartSent = true;
            } else {
                teardown();
                fail("stream hello failed");
            }
        }

        if (gClient && gConnected && gStartSent && gState != State::Failed) {
            PcmFrame frame;
            if (xQueueReceive(gAudio, &frame, 0) == pdTRUE) {
                const int sent = esp_websocket_client_send_bin(
                    gClient, reinterpret_cast<const char*>(frame.samples), FRAME_BYTES,
                    pdMS_TO_TICKS(150));
                if (sent != static_cast<int>(FRAME_BYTES)) {
                    teardown();
                    fail("audio stream send failed");
                } else {
                    ++gFramesSent;
                    ++gFramesThisTurn;
                }
            } else if (gFinishRequested) {
                if (sendEnd()) {
                    gFinishRequested = false;
                    gState = State::Waiting;
                } else {
                    teardown();
                    fail("stream finish failed");
                }
            }
        }

        const uint32_t stack = uxTaskGetStackHighWaterMark(nullptr);
        if (!gStackMinWords || stack < gStackMinWords) gStackMinWords = stack;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

bool enqueueFrame(const PcmFrame& frame) {
    if (!gAudio || xQueueSend(gAudio, &frame, 0) != pdTRUE) {
        ++gFrameDrops;
        gState = State::Failed;
        gCancelRequested = true;
        return false;
    }
    ++gFramesQueued;
    const auto depth = static_cast<uint8_t>(uxQueueMessagesWaiting(gAudio));
    if (depth > gQueueHighWater) gQueueHighWater = depth;
    return true;
}

std::string newTurnId() {
    char id[40];
    snprintf(id, sizeof(id), "%08lx-%08lx-%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(millis()),
             static_cast<unsigned long>(gTurns + 1));
    return id;
}

}  // namespace

bool begin() {
    if (gTask) return true;
    gAudio = xQueueCreate(AUDIO_CAPACITY, sizeof(PcmFrame));
    gEvents = xQueueCreate(EVENT_CAPACITY, sizeof(Event*));
    gStarts = xQueueCreate(START_CAPACITY, sizeof(StartRequest*));
    if (!gAudio || !gEvents || !gStarts) return false;
    return xTaskCreate(worker, "maz-ws", 6144, nullptr, 2, &gTask) == pdPASS;
}

bool start(const std::string& session, const char* route) {
    if (!gTask && !begin()) return false;
    if (Cfg.hostAddr.empty() || Cfg.hostToken.empty() || WiFi.status() != WL_CONNECTED)
        return false;
    if (gState == State::Connecting || gState == State::Streaming ||
        gState == State::Finishing || gState == State::Waiting)
        return false;

    xQueueReset(gAudio);
    gPartialCount = 0;
    gFinishRequested = false;
    gCancelRequested = false;
    gState = State::Connecting;

    StartRequest* request = new StartRequest();
    if (!request) return false;
    request->session = session;
    request->route = route ? route : "local";
    if (request->route != "local" && request->route != "auto" && request->route != "cloud")
        request->route = "local";
    request->turn = newTurnId();
    gActiveTurn = request->turn;
    if (xQueueSend(gStarts, &request, 0) != pdTRUE) {
        delete request;
        gState = State::Failed;
        return false;
    }
    ++gTurns;
    return true;
}

bool pushPcm(const int16_t* samples, size_t count) {
    if (!samples || !count) return true;
    if (gState == State::Failed || gState == State::Cancelled ||
        gState == State::Idle || gState == State::Done)
        return false;

    size_t pos = 0;
    while (pos < count) {
        const size_t take = std::min(FRAME_SAMPLES - gPartialCount, count - pos);
        memcpy(&gPartial.samples[gPartialCount], &samples[pos], take * sizeof(int16_t));
        gPartialCount += take;
        pos += take;
        if (gPartialCount == FRAME_SAMPLES) {
            if (!enqueueFrame(gPartial)) return false;
            gPartialCount = 0;
        }
    }
    return true;
}

void finish() {
    if (gState == State::Failed || gState == State::Cancelled || gState == State::Idle)
        return;
    if (gPartialCount) {
        memset(&gPartial.samples[gPartialCount], 0,
               (FRAME_SAMPLES - gPartialCount) * sizeof(int16_t));
        if (!enqueueFrame(gPartial)) return;
        gPartialCount = 0;
    }
    gFinishRequested = true;
    gState = State::Finishing;
}

void cancel() {
    gCancelRequested = true;
    gPartialCount = 0;
    if (gAudio) xQueueReset(gAudio);
}

void reset() {
    cancel();
    gState = State::Idle;
}

bool poll(Event& out) {
    if (!gEvents) return false;
    Event* event = nullptr;
    if (xQueueReceive(gEvents, &event, 0) != pdTRUE || !event) return false;
    out = *event;
    delete event;
    return true;
}

State state() { return gState; }
bool usable() {
    return gState == State::Connecting || gState == State::Streaming ||
           gState == State::Finishing || gState == State::Waiting;
}
bool failed() { return gState == State::Failed; }
std::string sessionId() { return gActiveSession; }
std::string turnId() { return gActiveTurn; }

Stats stats() {
    Stats out;
    out.turns = gTurns;
    out.framesQueued = gFramesQueued;
    out.framesSent = gFramesSent;
    out.frameDrops = gFrameDrops;
    out.reconnects = gReconnects;
    out.protocolErrors = gProtocolErrors;
    out.lastFirstTokenMs = gLastFirstToken;
    out.lastTotalMs = gLastTotal;
    out.stackMinWords = gStackMinWords;
    out.queueDepth = gAudio ? static_cast<uint8_t>(uxQueueMessagesWaiting(gAudio)) : 0;
    out.queueHighWater = gQueueHighWater;
    return out;
}

}  // namespace comm_stream
}  // namespace maz
