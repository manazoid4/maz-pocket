#include "mazhost.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "../core/settings.h"
#include "../core/sys.h"
#include "../storage/store.h"

namespace maz {
namespace host {
namespace {

std::string url(const char* path) {
    return "http://" + Cfg.hostAddr + ":" + std::to_string(Cfg.hostPort) + path;
}

const char* route() {
    return Cfg.talkRoute == 0 ? "local" : (Cfg.talkRoute == 2 ? "cloud" : "auto");
}

void beginRequest(HTTPClient& http, const char* path) {
    http.setConnectTimeout(3000);
    http.setTimeout(60000);
    http.begin(url(path).c_str());
    http.addHeader("Authorization", ("Bearer " + Cfg.hostToken).c_str());
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
        if (out.text.empty() && doc["result"].is<const char*>()) out.text = doc["result"].as<const char*>();
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
        out.error = error ? "invalid host response" : static_cast<const char*>(doc["detail"] | "host request failed");
    }
    http.end();
    Sys.hostOnline = out.ok || status == 400 || status == 404;
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
    if (!file) { out.error = "recording missing"; return out; }
    HTTPClient http;
    beginRequest(http, path);
    http.addHeader("Content-Type", "audio/wav");
    for (const auto& header : headers) http.addHeader(header.first.c_str(), header.second.c_str());
    const int status = http.sendRequest("POST", &file, file.size());
    file.close();
    return decodeReply(http, status);
}

Reply jsonPost(const char* path, const std::string& json) {
    Reply out;
    if (!configured() || WiFi.status() != WL_CONNECTED) { out.error = "host offline"; return out; }
    HTTPClient http;
    beginRequest(http, path);
    http.addHeader("Content-Type", "application/json");
    return decodeReply(http, http.POST(json.c_str()));
}

}  // namespace

bool configured() { return !Cfg.hostAddr.empty() && !Cfg.hostToken.empty(); }

bool health() {
    if (!configured() || WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    beginRequest(http, "/health");
    const int status = http.GET();
    http.end();
    Sys.hostOnline = status == 200;
    return Sys.hostOnline;
}

std::string startSession() {
    if (!configured() || WiFi.status() != WL_CONNECTED) return "";
    HTTPClient http;
    beginRequest(http, "/session/start");
    const int status = http.POST("");
    const String body = http.getString();
    http.end();
    JsonDocument doc;
    if (status != 200 || deserializeJson(doc, body)) return "";
    return doc["session_id"] | "";
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

Reply brainDump(const std::string& wavPath, const std::vector<uint32_t>& highlights) {
    String marks = "[";
    for (size_t i = 0; i < highlights.size(); ++i) {
        if (i) marks += ',';
        marks += highlights[i];
    }
    marks += ']';
    return upload("/braindump/raw", wavPath, {{"X-MAZ-Highlights", marks.c_str()}});
}

Assurance assurance() {
    Assurance out;
    if (!configured() || WiFi.status() != WL_CONNECTED) { out.error = "host offline"; return out; }
    HTTPClient http;
    beginRequest(http, "/nudge");
    const int status = http.GET();
    const String body = http.getString();
    http.end();
    JsonDocument doc;
    if (status != 200 || deserializeJson(doc, body)) { out.error = "nudge unavailable"; return out; }
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
    Sys.hostOnline = true;
    Sys.agentsWorking = out.working;
    Sys.agentsWaiting = out.waiting;
    Sys.agentsStale = out.needsNudge + out.overdue;
    Sys.agentQuestion = out.questionForMaz > 0;
    Sys.nudgeDue = out.needsNudge + out.overdue > 0;
    return out;
}

Reply sendNudge(const std::string& sessionId) {
    return jsonPost(("/nudge/" + sessionId + "/nudge").c_str(), "{}");
}

}  // namespace host
}  // namespace maz
