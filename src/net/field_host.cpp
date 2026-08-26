// v0.7 FIELD host calls kept separate from the shipped COMM/Core clients.
// These endpoints move only bounded text/status JSON. Received Beam text is
// always data; no path in this file can execute it.
#include "mazhost.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "../core/settings.h"
#include "../core/sys.h"

namespace maz {
namespace host {
namespace {

static const char ISRG_ROOT_X1_FIELD[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

std::vector<std::pair<std::string, bool>> fieldBases() {
    std::vector<std::pair<std::string, bool>> out;
    if (!Cfg.hostAddr.empty())
        out.push_back({"http://" + Cfg.hostAddr + ":" + std::to_string(Cfg.hostPort), false});
    if (!Cfg.hostRemoteUrl.empty()) {
        std::string remote = Cfg.hostRemoteUrl;
        while (!remote.empty() && remote.back() == '/') remote.pop_back();
        if (remote.rfind("https://", 0) == 0) out.push_back({remote, true});
    }
    return out;
}

bool beginField(HTTPClient& http, const std::string& base, const char* path, bool remote) {
    if (Cfg.hostToken.empty() || WiFi.status() != WL_CONNECTED) return false;
    http.setConnectTimeout(remote ? 6500 : 1800);
    http.setTimeout(12000);
    const std::string url = base + path;
    const bool begun = remote ? http.begin(url.c_str(), ISRG_ROOT_X1_FIELD)
                              : http.begin(url.c_str());
    if (!begun) return false;
    http.addHeader("Authorization", ("Bearer " + Cfg.hostToken).c_str());
    return true;
}

std::string jsonError(JsonDocument& doc, const char* fallback) {
    if (doc["detail"].is<const char*>()) return doc["detail"].as<const char*>();
    if (doc["error"].is<const char*>()) return doc["error"].as<const char*>();
    return fallback;
}

enum class BodyReadResult : uint8_t { Ok, TooLarge, Incomplete };

BodyReadResult readBoundedBody(HTTPClient& http, String& body, size_t maxBytes) {
    const int expected = http.getSize();
    if (expected > 0 && static_cast<size_t>(expected) > maxBytes)
        return BodyReadResult::TooLarge;

    body = "";
    body.reserve(expected > 0 ? static_cast<size_t>(expected) : 512u);
    WiFiClient* stream = http.getStreamPtr();
    if (!stream) return BodyReadResult::Incomplete;

    uint8_t chunk[256];
    uint32_t lastProgress = millis();
    while (http.connected() &&
           (expected < 0 || body.length() < static_cast<size_t>(expected))) {
        const size_t available = static_cast<size_t>(stream->available());
        if (!available) {
            if (millis() - lastProgress > 12000u) break;
            delay(1);
            continue;
        }
        size_t wanted = std::min(available, sizeof(chunk));
        if (expected >= 0) {
            const size_t remaining = static_cast<size_t>(expected) - body.length();
            wanted = std::min(wanted, remaining);
        }
        if (body.length() + wanted > maxBytes) return BodyReadResult::TooLarge;
        const int count = stream->readBytes(chunk, wanted);
        if (count <= 0) break;
        body.concat(reinterpret_cast<const char*>(chunk), static_cast<unsigned int>(count));
        lastProgress = millis();
    }

    if (body.length() > maxBytes) return BodyReadResult::TooLarge;
    if (expected >= 0 && body.length() != static_cast<size_t>(expected))
        return BodyReadResult::Incomplete;
    return BodyReadResult::Ok;
}

void fieldOnline() { Sys.hostOnline = true; }

}  // namespace

Reply talkTextContext(const std::string& session, const std::string& text,
                      const std::string& context) {
    Reply out;
    if (session.empty() || text.empty()) { out.error = "context ask missing input"; return out; }
    JsonDocument req;
    req["session_id"] = session;
    req["route"] = talkRouteApiName(Cfg.talkRoute);
    req["text"] = text;
    req["context"] = context.size() > 700 ? context.substr(0, 700) : context;
    String payload;
    serializeJson(req, payload);

    for (const auto& base : fieldBases()) {
        HTTPClient http;
        if (!beginField(http, base.first, "/turn/text", base.second)) continue;
        http.addHeader("Content-Type", "application/json");
        const int status = http.POST(payload);
        const String body = status > 0 ? http.getString() : String();
        http.end();
        if (status <= 0) continue;
        out.status = status;
        JsonDocument doc;
        if (deserializeJson(doc, body)) { out.error = "invalid Context Ask response"; return out; }
        if (status < 200 || status >= 300) { out.error = jsonError(doc, "Context Ask failed"); return out; }
        fieldOnline();
        out.ok = true;
        out.text = doc["reply"] | "";
        out.transcript = doc["text"] | text.c_str();
        out.provider = doc["provider"] | "";
        JsonArray commands = doc["commands"].as<JsonArray>();
        if (!commands.isNull() && commands.size() &&
            std::string(commands[0]["type"] | "") == "reminder.create") {
            out.reminderTitle = commands[0]["title"] | "";
            out.reminderDelay = commands[0]["delay_seconds"] | 0;
        }
        return out;
    }
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

Reply beamSend(const std::string& text) {
    Reply out;
    if (text.empty()) { out.error = "empty Beam"; return out; }
    JsonDocument req;
    req["text"] = text.size() > 2000 ? text.substr(0, 2000) : text;
    String payload;
    serializeJson(req, payload);

    for (const auto& base : fieldBases()) {
        HTTPClient http;
        if (!beginField(http, base.first, "/beam/from-pocket", base.second)) continue;
        http.addHeader("Content-Type", "application/json");
        const int status = http.POST(payload);
        const String body = status > 0 ? http.getString() : String();
        http.end();
        if (status <= 0) continue;
        out.status = status;
        JsonDocument doc;
        if (deserializeJson(doc, body)) { out.error = "invalid Beam response"; return out; }
        if (status < 200 || status >= 300) { out.error = jsonError(doc, "Beam failed"); return out; }
        fieldOnline();
        out.ok = true;
        out.text = doc["reply"] | "BEAMED TO LAPTOP";
        out.provider = "beam-local";
        return out;
    }
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

BeamMessage beamPull() {
    BeamMessage out;
    for (const auto& base : fieldBases()) {
        HTTPClient http;
        if (!beginField(http, base.first, "/beam/pull", base.second)) continue;
        const int status = http.GET();
        const String body = status > 0 ? http.getString() : String();
        http.end();
        if (status <= 0) continue;
        JsonDocument doc;
        if (deserializeJson(doc, body)) { out.error = "invalid Beam response"; return out; }
        if (status < 200 || status >= 300) { out.error = jsonError(doc, "Beam unavailable"); return out; }
        fieldOnline();
        out.ok = true;
        out.hasMessage = doc["message"].is<JsonObject>();
        if (out.hasMessage) {
            JsonObject msg = doc["message"].as<JsonObject>();
            out.id = msg["id"] | "";
            out.kind = msg["kind"] | "text";
            out.text = msg["text"] | "";
        }
        return out;
    }
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

SystemStatus systemStatus() {
    SystemStatus out;
    for (const auto& base : fieldBases()) {
        HTTPClient http;
        if (!beginField(http, base.first, "/system/status", base.second)) continue;
        const int status = http.GET();
        const String body = status > 0 ? http.getString() : String();
        http.end();
        if (status <= 0) continue;
        JsonDocument doc;
        if (deserializeJson(doc, body)) { out.error = "invalid laptop status"; return out; }
        if (status < 200 || status >= 300) { out.error = jsonError(doc, "status unavailable"); return out; }
        fieldOnline();
        out.ok = doc["ok"] | true;
        out.cpuPct = doc["cpu_pct"].isNull() ? -1 : static_cast<int>(doc["cpu_pct"].as<float>() + 0.5f);
        out.ramPct = doc["ram_pct"].isNull() ? -1 : static_cast<int>(doc["ram_pct"].as<float>() + 0.5f);
        if (!doc["battery_pct"].isNull()) out.batteryPct = static_cast<int>(doc["battery_pct"].as<float>() + 0.5f);
        out.charging = doc["charging"] | false;
        JsonObject gpu = doc["gpu"].as<JsonObject>();
        out.gpuAvailable = !gpu.isNull() && (gpu["available"] | false);
        if (out.gpuAvailable) {
            out.gpuPct = gpu["util_pct"] | -1;
            out.vramUsedMb = gpu["vram_used_mb"] | 0;
            out.vramTotalMb = gpu["vram_total_mb"] | 0;
            out.gpuTempC = gpu["temp_c"] | -1;
        }
        JsonObject ollama = doc["ollama"].as<JsonObject>();
        if (!ollama.isNull()) {
            out.ollamaOnline = ollama["online"] | false;
            out.ollamaLoaded = ollama["loaded"] | false;
            out.ollamaModel = ollama["model"] | "";
            out.ollamaVramMb = ollama["vram_mb"] | 0;
            out.ollamaContext = ollama["context"] | 0;
        }
        return out;
    }
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

WorkSummary workSummary() {
    constexpr size_t MAX_WORK_PAYLOAD_BYTES = 4096;
    WorkSummary out;
    for (const auto& base : fieldBases()) {
        HTTPClient http;
        if (!beginField(http, base.first, "/work/cardputer", base.second)) continue;
        const int status = http.GET();
        String body;
        const BodyReadResult read = status > 0
                                        ? readBoundedBody(http, body, MAX_WORK_PAYLOAD_BYTES)
                                        : BodyReadResult::Incomplete;
        http.end();
        if (status <= 0) continue;
        if (read == BodyReadResult::TooLarge) {
            out.error = "work payload too large";
            return out;
        }
        if (read != BodyReadResult::Ok) {
            out.error = "incomplete work response";
            return out;
        }
        JsonDocument doc;
        if (deserializeJson(doc, body)) { out.error = "invalid work response"; return out; }
        if (status < 200 || status >= 300) { out.error = jsonError(doc, "work unavailable"); return out; }

        // Fail the whole snapshot if any required field is absent or malformed.
        // finishWork() then retains the previous successful data instead of
        // replacing unknown values with default zeroes.
        if (!doc["ok"].is<bool>() || !doc["ok"].as<bool>() ||
            !doc["tracks"].is<JsonArray>() || !doc["seven_day"].is<JsonArray>()) {
            out.error = "malformed work response";
            return out;
        }
        JsonArray tracks = doc["tracks"].as<JsonArray>();
        if (tracks.size() > WORK_MAX_TRACKS) {
            out.error = "too many work tracks";
            return out;
        }
        int i = 0;
        for (JsonObject t : tracks) {
            const char* id = t["track_id"] | nullptr;
            const char* label = t["short_label"] | nullptr;
            if (!id || !label || !id[0] || !label[0] || strlen(id) > 80 ||
                strlen(label) > 16 || !t["today_total"].is<float>()) {
                out.error = "malformed work track";
                return out;
            }
            const float today = t["today_total"].as<float>();
            if (!std::isfinite(today)) { out.error = "invalid work total"; return out; }
            out.tracks[i].id = id;
            out.tracks[i].shortLabel = label;
            out.tracks[i].todayTotal = today;
            out.tracks[i].hasTarget = !t["target"].isNull();
            if (out.tracks[i].hasTarget) {
                if (!t["target"].is<float>()) { out.error = "invalid work target"; return out; }
                out.tracks[i].target = t["target"].as<float>();
                if (!std::isfinite(out.tracks[i].target)) {
                    out.error = "invalid work target";
                    return out;
                }
            }
            ++i;
        }
        out.trackCount = i;
        JsonArray seven = doc["seven_day"].as<JsonArray>();
        if (seven.size() != WORK_HISTORY_DAYS) {
            out.error = "invalid work history length";
            return out;
        }
        int d = 0;
        for (JsonVariant dayValue : seven) {
            if (!dayValue.is<JsonObject>()) { out.error = "invalid work history"; return out; }
            JsonObject day = dayValue.as<JsonObject>();
            if (day.size() > WORK_MAX_TRACKS) { out.error = "work history too wide"; return out; }
            float total = 0;
            for (JsonPair kv : day) {
                if (!kv.value().is<float>()) { out.error = "invalid work history total"; return out; }
                const float value = kv.value().as<float>();
                if (!std::isfinite(value)) { out.error = "invalid work history total"; return out; }
                total += value;
            }
            out.sevenDay[d] = total;
            ++d;
        }
        out.ok = true;
        fieldOnline();
        return out;
    }
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

}  // namespace host
}  // namespace maz
