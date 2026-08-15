#include "mazhost.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <utility>
#include <vector>

#include "../core/settings.h"
#include "../core/sys.h"
#include "../storage/store.h"

namespace maz {
namespace host {
namespace {

bool gRemote = false;

// Tailscale Funnel provisions a normal public HTTPS certificate. Pin the
// current Let's Encrypt trust root rather than using setInsecure(): the shared
// MAZ bearer token must never be sent to an unauthenticated TLS peer.
static const char ISRG_ROOT_X1[] PROGMEM = R"EOF(
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

std::vector<std::pair<std::string, bool>> bases() {
    std::vector<std::pair<std::string, bool>> out;
    if (!Cfg.hostAddr.empty()) {
        out.push_back({"http://" + Cfg.hostAddr + ":" +
                           std::to_string(Cfg.hostPort),
                       false});
    }
    if (!Cfg.hostRemoteUrl.empty()) {
        std::string remote = Cfg.hostRemoteUrl;
        while (!remote.empty() && remote.back() == '/') remote.pop_back();
        if (remote.rfind("https://", 0) == 0) out.push_back({remote, true});
    }
    return out;
}

const char* route() {
    return Cfg.talkRoute == 0 ? "local" : (Cfg.talkRoute == 2 ? "cloud" : "auto");
}

bool beginRequest(HTTPClient& http, const std::string& base, const char* path,
                  bool remote) {
    http.setConnectTimeout(remote ? 6500 : 1800);
    http.setTimeout(60000);
    const String url = (base + path).c_str();
    const bool begun = remote ? http.begin(url, ISRG_ROOT_X1) : http.begin(url);
    if (!begun) return false;
    http.addHeader("Authorization", ("Bearer " + Cfg.hostToken).c_str());
    return true;
}

void markOnline(bool remote) {
    gRemote = remote;
    Sys.hostOnline = true;
}

Reply decodeReply(HTTPClient& http, int status) {
    Reply out;
    out.status = status;
    const String body = http.getString();
    JsonDocument doc;
    const auto error = deserializeJson(doc, body);
    if (status >= 200 && status < 300 && !error) {
        out.ok = true;
        out.text = doc["reply"] | "";
        if (out.text.empty() && doc["result"].is<const char*>())
            out.text = doc["result"].as<const char*>();
        if (out.text.empty()) out.text = doc["summary"] | "";
        out.transcript = doc["text"] | "";
        if (out.transcript.empty()) out.transcript = doc["transcript"] | "";
        out.provider = doc["provider"] | "";
        JsonArray commands = doc["commands"].as<JsonArray>();
        if (!commands.isNull() && commands.size() &&
            std::string(commands[0]["type"] | "") == "reminder.create") {
            out.reminderTitle = commands[0]["title"] | "";
            out.reminderDelay = commands[0]["delay_seconds"] | 0;
        }
    } else {
        out.error = error ? "invalid host response"
                          : static_cast<const char*>(doc["detail"] | "host request failed");
    }
    http.end();
    return out;
}

Reply upload(const char* path, const std::string& wavPath,
             const std::vector<std::pair<std::string, std::string>>& headers) {
    Reply out;
    if (!configured() || WiFi.status() != WL_CONNECTED || !store::ready()) {
        out.error = "host offline";
        return out;
    }
    File file = store::fs()->open(wavPath.c_str(), FILE_READ);
    if (!file) {
        out.error = "recording missing";
        return out;
    }

    for (const auto& base : bases()) {
        file.seek(0);
        HTTPClient http;
        if (!beginRequest(http, base.first, path, base.second)) continue;
        http.addHeader("Content-Type", "audio/wav");
        for (const auto& header : headers)
            http.addHeader(header.first.c_str(), header.second.c_str());
        const int status = http.sendRequest("POST", &file, file.size());
        if (status <= 0) {
            http.end();
            continue;
        }
        file.close();
        markOnline(base.second);
        return decodeReply(http, status);
    }
    file.close();
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

Reply jsonPost(const char* path, const std::string& json) {
    Reply out;
    if (!configured() || WiFi.status() != WL_CONNECTED) {
        out.error = "host offline";
        return out;
    }
    for (const auto& base : bases()) {
        HTTPClient http;
        if (!beginRequest(http, base.first, path, base.second)) continue;
        http.addHeader("Content-Type", "application/json");
        const int status = http.POST(json.c_str());
        if (status <= 0) {
            http.end();
            continue;
        }
        markOnline(base.second);
        return decodeReply(http, status);
    }
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

}  // namespace

bool configured() {
    return !Cfg.hostToken.empty() && (!Cfg.hostAddr.empty() || !Cfg.hostRemoteUrl.empty());
}

const char* linkName() {
    if (!Sys.hostOnline) return "OFFLINE";
    return gRemote ? "REMOTE" : "LAN";
}

bool health() {
    if (!configured() || WiFi.status() != WL_CONNECTED) {
        Sys.hostOnline = false;
        return false;
    }
    for (const auto& base : bases()) {
        HTTPClient http;
        if (!beginRequest(http, base.first, "/health", base.second)) continue;
        const int status = http.GET();
        http.end();
        if (status <= 0) continue;
        if (status == 200) {
            markOnline(base.second);
            return true;
        }
        Sys.hostOnline = false;
        return false;
    }
    Sys.hostOnline = false;
    return false;
}

std::string startSession() {
    if (!configured() || WiFi.status() != WL_CONNECTED) return "";
    for (const auto& base : bases()) {
        HTTPClient http;
        if (!beginRequest(http, base.first, "/session/start", base.second)) continue;
        const int status = http.POST("");
        if (status <= 0) {
            http.end();
            continue;
        }
        const String body = http.getString();
        http.end();
        JsonDocument doc;
        if (status != 200 || deserializeJson(doc, body)) return "";
        markOnline(base.second);
        return doc["session_id"] | "";
    }
    Sys.hostOnline = false;
    return "";
}

Reply talkText(const std::string& session, const std::string& text) {
    JsonDocument doc;
    doc["session_id"] = session;
    doc["route"] = route();
    doc["text"] = text;
    String body;
    serializeJson(doc, body);
    return jsonPost("/turn/text", body.c_str());
}

Reply talkAudio(const std::string& session, const std::string& wavPath) {
    return upload("/turn/raw", wavPath,
                  {{"X-MAZ-Session", session}, {"X-MAZ-Route", route()}});
}

Reply transcribe(const std::string& wavPath) {
    return upload("/transcribe/raw", wavPath, {});
}

Reply brainDump(const std::string& wavPath,
                const std::vector<uint32_t>& highlights) {
    String marks = "[";
    for (size_t i = 0; i < highlights.size(); ++i) {
        if (i) marks += ',';
        marks += highlights[i];
    }
    marks += ']';
    return upload("/braindump/raw", wavPath,
                  {{"X-MAZ-Highlights", marks.c_str()}});
}

Reply pcAction(const std::string& action) {
    JsonDocument doc;
    doc["action"] = action;
    String body;
    serializeJson(doc, body);
    return jsonPost("/pc/action", body.c_str());
}

bool speak(const std::string& text, const std::string& wavPath) {
    if (!configured() || WiFi.status() != WL_CONNECTED || !store::ready() ||
        text.empty())
        return false;

    JsonDocument doc;
    doc["text"] = text.size() > 1400 ? text.substr(0, 1400) : text;
    String body;
    serializeJson(doc, body);

    for (const auto& base : bases()) {
        HTTPClient http;
        if (!beginRequest(http, base.first, "/speak", base.second)) continue;
        http.addHeader("Content-Type", "application/json");
        const int status = http.POST(body);
        if (status <= 0) {
            http.end();
            continue;
        }
        if (status != 200) {
            http.end();
            return false;
        }
        store::remove(wavPath);
        File out = store::fs()->open(wavPath.c_str(), FILE_WRITE);
        if (!out) {
            http.end();
            return false;
        }
        const int written = http.writeToStream(&out);
        out.close();
        http.end();
        if (written > 44) {
            markOnline(base.second);
            return true;
        }
        store::remove(wavPath);
        return false;
    }
    return false;
}

Assurance assurance() {
    Assurance out;
    if (!configured() || WiFi.status() != WL_CONNECTED) {
        out.error = "host offline";
        return out;
    }
    for (const auto& base : bases()) {
        HTTPClient http;
        if (!beginRequest(http, base.first, "/nudge", base.second)) continue;
        const int status = http.GET();
        if (status <= 0) {
            http.end();
            continue;
        }
        const String body = http.getString();
        http.end();
        JsonDocument doc;
        if (status != 200 || deserializeJson(doc, body)) {
            out.error = "nudge unavailable";
            return out;
        }
        markOnline(base.second);
        out.ok = true;
        out.state = doc["state"] | "ALL_SYNCED";
        out.working = doc["counts"]["working"] | 0;
        out.waiting = doc["counts"]["waiting"] | 0;
        out.needsNudge = doc["counts"]["needsNudge"] | 0;
        out.overdue = doc["counts"]["overdue"] | 0;
        out.attention = doc["counts"]["attention"] | 0;
        out.questionForMaz = doc["counts"]["questionForMaz"] | 0;
        for (JsonObject item : doc["agents"].as<JsonArray>()) {
            Agent a;
            a.id = item["sessionId"] | "";
            a.provider = item["provider"] | "agent";
            a.name = item["projectName"] | "";
            if (a.name.empty()) a.name = item["projectId"] | a.provider.c_str();
            a.state = item["state"] | "ALL_SYNCED";
            a.sessionState = item["sessionState"] | "offline";
            JsonArray refs = item["evidenceRefs"].as<JsonArray>();
            if (!refs.isNull() && refs.size()) a.evidence = refs[0].as<const char*>();
            out.agents.push_back(a);
        }
        Sys.agentsWorking = out.working;
        Sys.agentsWaiting = out.waiting;
        Sys.agentsStale = out.needsNudge + out.overdue;
        Sys.agentQuestion = out.questionForMaz > 0;
        Sys.nudgeDue = out.needsNudge + out.overdue > 0;
        return out;
    }
    Sys.hostOnline = false;
    out.error = "PC unreachable";
    return out;
}

Reply sendNudge(const std::string& sessionId) {
    return jsonPost(("/nudge/" + sessionId + "/nudge").c_str(), "{}");
}

}  // namespace host
}  // namespace maz
