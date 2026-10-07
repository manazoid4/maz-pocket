#include "mazhost.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "remote_tls.h"
#include <WiFi.h>
#include <WiFiUdp.h>

#include <utility>
#include <vector>

#include "../core/settings.h"
#include "../core/sys.h"
#include "../storage/store.h"

namespace maz {
namespace host {
namespace {

bool gRemote = false;


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
    return talkRouteApiName(Cfg.talkRoute);
}

bool beginRequest(NodHttp& http, const std::string& base, const char* path,
                  bool remote) {
    http.setConnectTimeout(remote ? 6500 : 1800);
    http.setTimeout(60000);
    const String url = (base + path).c_str();
    const bool begun = http.open(url, remote);
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
        Serial.printf("[upload] cannot open %s\n", wavPath.c_str());
        out.error = "recording missing";
        return out;
    }

    for (const auto& base : bases()) {
        file.seek(0);
        NodHttp http;
        if (!beginRequest(http, base.first, path, base.second)) continue;
        http.addHeader("Content-Type", "audio/wav");
        for (const auto& header : headers)
            http.addHeader(header.first.c_str(), header.second.c_str());
        const int status = http.sendRequest("POST", &file, file.size());
        Serial.printf("[upload] %s%s size=%u status=%d\n", base.first.c_str(), path,
                      static_cast<unsigned>(file.size()), status);
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
        NodHttp http;
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

bool onRemoteLink() { return Sys.hostOnline && gRemote; }

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
        NodHttp http;
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
        NodHttp http;
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

bool probeCore(const std::string& addr, uint16_t port, int timeoutMs) {
    if (addr.empty() || WiFi.status() != WL_CONNECTED) return false;
    WiFiClient c;
    if (!c.connect(addr.c_str(), port, timeoutMs)) return false;
    c.print("GET /health HTTP/1.0\r\nHost: nod\r\nConnection: close\r\n\r\n");
    String resp;
    const uint32_t t0 = millis();
    while (millis() - t0 < 700 && resp.length() < 700 && (c.connected() || c.available())) {
        while (c.available() && resp.length() < 700) resp += static_cast<char>(c.read());
        delay(4);
    }
    c.stop();
    return resp.indexOf("nod Core") >= 0;
}

namespace {
// Ask the LAN "where is Core?". Core answers the datagram with "NOD-CORE {...}" from its own address.
std::string broadcastFind(uint16_t port) {
    WiFiUDP udp;
    if (!udp.begin(48787)) return "";
    const IPAddress targets[2] = {IPAddress(255, 255, 255, 255), WiFi.broadcastIP()};
    for (const auto& ip : targets) {
        udp.beginPacket(ip, port);
        udp.write(reinterpret_cast<const uint8_t*>("NOD-CORE?"), 9);
        udp.endPacket();
    }
    const uint32_t t0 = millis();
    std::string ip;
    while (millis() - t0 < 1400) {
        const int n = udp.parsePacket();
        if (n > 0) {
            char buf[40] = {};
            udp.read(buf, sizeof(buf) - 1);
            if (!strncmp(buf, "NOD-CORE ", 9)) { ip = udp.remoteIP().toString().c_str(); break; }
        }
        delay(20);
    }
    udp.stop();
    return ip;
}
}  // namespace

bool FindCore::step() {
    if (phase == Done) return true;
    if (WiFi.status() != WL_CONNECTED) { phase = Done; return true; }
    if (phase == Known) {
        phase = Broadcast;
        if (!Cfg.hostAddr.empty() && probeCore(Cfg.hostAddr, port, 600)) { found = Cfg.hostAddr; phase = Done; }
        return phase == Done;
    }
    if (phase == Broadcast) {
        phase = Scan;
        const std::string ip = broadcastFind(port);
        if (!ip.empty() && probeCore(ip, port, 600)) { found = ip; phase = Done; }
        return phase == Done;
    }
    const IPAddress me = WiFi.localIP();
    for (int i = 0; i < 6 && next <= 254; ++i, ++next) {
        if (next == static_cast<int>(me[3])) continue;
        char ip[20];
        snprintf(ip, sizeof(ip), "%u.%u.%u.%d", me[0], me[1], me[2], next);
        if (probeCore(ip, port, 140)) { found = ip; phase = Done; return true; }
    }
    if (next > 254) phase = Done;
    return phase == Done;
}

int FindCore::percent() const {
    switch (phase) {
        case Known: return 2;
        case Broadcast: return 8;
        case Scan: return 10 + (next * 90) / 255;
        default: return 100;
    }
}

ClaimResult pairClaim(const std::string& addr, uint16_t port, const std::string& code) {
    if (WiFi.status() != WL_CONNECTED || addr.empty()) return ClaimResult::NotFound;
    JsonDocument req;
    req["code"] = code;
    String body;
    serializeJson(req, body);
    HTTPClient http;
    if (!http.begin(("http://" + addr + ":" + std::to_string(port) + "/pair/claim").c_str()))
        return ClaimResult::NotFound;
    http.setConnectTimeout(3000);
    http.setTimeout(8000);
    http.addHeader("Content-Type", "application/json");
    const int status = http.POST(body);
    const String resp = status > 0 ? http.getString() : String();
    http.end();
    if (status <= 0) return ClaimResult::NotFound;
    if (status == 400) return ClaimResult::WrongCode;
    if (status == 429) return ClaimResult::TooMany;
    if (status == 403) return ClaimResult::Refused;
    if (status != 200) return ClaimResult::BadReply;
    JsonDocument doc;
    if (deserializeJson(doc, resp)) return ClaimResult::BadReply;
    const std::string token = doc["token"] | "";
    if (token.size() < 8 || token.size() > 200) return ClaimResult::BadReply;
    Cfg.hostAddr = addr;
    Cfg.hostPort = port;
    Cfg.hostToken = token;
    Cfg.firstRunComplete = true;
    Cfg.save();
    Sys.hostAddr = Cfg.hostAddr;
    Sys.hostPort = Cfg.hostPort;
    health();
    return ClaimResult::Ok;
}

PairCode startPairing() {
    PairCode out;
    if (!configured() || WiFi.status() != WL_CONNECTED) {
        out.error = "MAZ Core offline";
        return out;
    }
    for (const auto& base : bases()) {
        NodHttp http;
        if (!beginRequest(http, base.first, "/pair/start", base.second)) continue;
        const int status = http.POST("");
        if (status <= 0) {
            http.end();
            continue;
        }
        const String body = http.getString();
        http.end();
        JsonDocument doc;
        if (deserializeJson(doc, body)) {
            out.error = "invalid pairing response";
            return out;
        }
        if (status < 200 || status >= 300) {
            out.error = doc["detail"] | "pairing unavailable";
            return out;
        }
        const std::string code = doc["code"] | "";
        if (code.size() != 8) {
            out.error = "invalid pairing code";
            return out;
        }
        markOnline(base.second);
        out.ok = true;
        out.code = code;
        out.expiresInSeconds = doc["expires_in_seconds"] | 300;
        return out;
    }
    Sys.hostOnline = false;
    out.error = "MAZ Core unreachable";
    return out;
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

namespace {
// POST a JSON body to a Core route that answers with a WAV, save it to wavPath.
bool postWav(const char* path, const std::string& body, const std::string& wavPath) {
    if (!configured() || WiFi.status() != WL_CONNECTED || !store::ready()) return false;

    for (const auto& base : bases()) {
        NodHttp http;
        if (!beginRequest(http, base.first, path, base.second)) continue;
        http.addHeader("Content-Type", "application/json");
        const int status = http.POST(body.c_str());
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
        Serial.printf("[speak] path=%s status=%d bytes=%d\n", wavPath.c_str(), status, written);
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
}  // namespace

bool speak(const std::string& text, const std::string& wavPath) {
    if (text.empty()) return false;
    JsonDocument doc;
    doc["text"] = text.size() > 1400 ? text.substr(0, 1400) : text;
    String body;
    serializeJson(doc, body);
    return postWav("/speak", body.c_str(), wavPath);
}

bool voiceList(std::vector<VoiceItem>& out, std::string& error) {
    out.clear();
    if (!configured() || WiFi.status() != WL_CONNECTED) {
        error = "host offline";
        return false;
    }
    for (const auto& base : bases()) {
        NodHttp http;
        if (!beginRequest(http, base.first, "/voices", base.second)) continue;
        const int status = http.GET();
        if (status <= 0) {
            http.end();
            continue;
        }
        const String body = http.getString();
        http.end();
        JsonDocument doc;
        if (status != 200 || deserializeJson(doc, body)) {
            error = "voices unavailable";
            return false;
        }
        markOnline(base.second);
        for (JsonObject v : doc["voices"].as<JsonArray>()) {
            VoiceItem item;
            item.name = v["name"] | "";
            item.id = v["reference_id"] | "";
            item.description = v["description"] | "";
            item.current = v["current"] | false;
            if (!item.id.empty()) out.push_back(item);
        }
        return !out.empty();
    }
    error = "PC unreachable";
    return false;
}

bool voiceSelect(const std::string& id, const std::string& name) {
    JsonDocument doc;
    doc["reference_id"] = id;
    doc["name"] = name;
    String body;
    serializeJson(doc, body);
    return jsonPost("/voices/select", body.c_str()).ok;
}

bool voicePreview(const std::string& id, const std::string& wavPath) {
    JsonDocument doc;
    doc["reference_id"] = id;
    String body;
    serializeJson(doc, body);
    return postWav("/voices/preview", body.c_str(), wavPath);
}

Assurance assurance() {
    Assurance out;
    if (!configured() || WiFi.status() != WL_CONNECTED) {
        out.error = "host offline";
        return out;
    }
    for (const auto& base : bases()) {
        NodHttp http;
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
