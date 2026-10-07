#include "mazhost.h"

#include <cstring>
#include <utility>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "remote_tls.h"

#include "../core/settings.h"
#include "../core/sys.h"

namespace maz {
namespace host {
namespace {


const char* routeName() {
    return talkRouteApiName(Cfg.talkRoute);
}

bool openHttp(HTTPClient& http, WiFiClient& plain, WiFiClientSecure& secure,
              const std::string& base, bool remote, const std::string& path) {
    const String url = (base + path).c_str();
    bool begun = false;
    if (remote) {
        secure.setCACertBundle(nodCaBundle);
        begun = http.begin(secure, url);
    } else {
        begun = http.begin(plain, url);
    }
    if (!begun) return false;
    http.setConnectTimeout(remote ? 6500 : 1800);
    // Arduino HTTPClient stores timeout as uint16_t. Keep this below 65.5s;
    // longer work belongs in MAZ Core jobs rather than one handheld request.
    http.setTimeout(60000);
    http.addHeader("Authorization", ("Bearer " + Cfg.hostToken).c_str());
    return true;
}

struct RawResponse {
    int status = 0;
    String body;
};

RawResponse requestOne(const std::string& base, bool remote, const std::string& path,
                       const char* method, const String& body = "") {
    HTTPClient http;
    WiFiClient plain;
    WiFiClientSecure secure;
    RawResponse out;
    if (!openHttp(http, plain, secure, base, remote, path)) return out;
    if (!strcmp(method, "GET")) out.status = http.GET();
    else {
        http.addHeader("Content-Type", "application/json");
        out.status = http.POST(body);
    }
    if (out.status > 0) out.body = http.getString();
    http.end();
    return out;
}

RawResponse requestCore(const std::string& path, const char* method, const String& body = "") {
    if (!configured() || WiFi.status() != WL_CONNECTED) return {};
    if (!Cfg.hostAddr.empty()) {
        const std::string local = "http://" + Cfg.hostAddr + ":" + std::to_string(Cfg.hostPort);
        RawResponse response = requestOne(local, false, path, method, body);
        if (response.status > 0) {
            Sys.hostOnline = response.status >= 200 && response.status < 500;
            return response;
        }
    }
    if (!Cfg.hostRemoteUrl.empty()) {
        std::string remote = Cfg.hostRemoteUrl;
        while (!remote.empty() && remote.back() == '/') remote.pop_back();
        if (remote.rfind("https://", 0) == 0) {
            RawResponse response = requestOne(remote, true, path, method, body);
            if (response.status > 0) {
                Sys.hostOnline = response.status >= 200 && response.status < 500;
                return response;
            }
        }
    }
    Sys.hostOnline = false;
    return {};
}

Reply decodeReply(const RawResponse& raw) {
    Reply out;
    out.status = raw.status;
    JsonDocument doc;
    const auto error = deserializeJson(doc, raw.body);
    if (raw.status >= 200 && raw.status < 300 && !error) {
        out.ok = true;
        out.text = doc["reply"] | "";
        out.provider = doc["provider"] | "";
    } else {
        out.error = raw.status <= 0 ? "hub unreachable"
            : (error ? "invalid hub response" : static_cast<const char*>(doc["detail"] | "hub request failed"));
    }
    return out;
}

Reply workPost(const char* path, JsonDocument& doc) {
    doc["route"] = routeName();
    String body;
    serializeJson(doc, body);
    return decodeReply(requestCore(path, "POST", body));
}

TeachStatus decodeTeach(const RawResponse& raw) {
    TeachStatus out;
    JsonDocument doc;
    const auto error = deserializeJson(doc, raw.body);
    if (raw.status < 200 || raw.status >= 300 || error) {
        out.error = raw.status <= 0 ? "hub unreachable"
            : (error ? "invalid Teach response" : static_cast<const char*>(doc["detail"] | "Teach failed"));
        return out;
    }
    JsonObjectConst root = doc.as<JsonObjectConst>();
    JsonObjectConst session = root["session"].is<JsonObjectConst>()
        ? root["session"].as<JsonObjectConst>() : root;
    out.ok = true;
    out.sessionId = session["session_id"] | "";
    out.state = session["state"] | "";
    out.transcript = session["transcript"] | "";
    out.error = session["error"] | "";
    out.frameCount = session["frame_count"] | 0;
    out.elapsedSeconds = static_cast<int>(session["elapsed_seconds"] | 0.0);
    return out;
}

}  // namespace

Reply workPlan(const std::string& task, const std::string& project) {
    JsonDocument doc; doc["task"] = task; doc["project"] = project;
    return workPost("/work/plan", doc);
}
Reply workCrew(const std::string& task, const std::string& project) {
    JsonDocument doc; doc["task"] = task; doc["project"] = project;
    return workPost("/work/crew", doc);
}
Reply workRetro(const std::string& project, const std::string& note) {
    JsonDocument doc; doc["project"] = project; doc["note"] = note;
    return workPost("/work/retro", doc);
}
Reply workPrompt(const std::string& templateId, const std::string& task,
                 const std::string& project) {
    JsonDocument doc; doc["template_id"] = templateId; doc["task"] = task; doc["project"] = project;
    return workPost("/work/prompt", doc);
}

std::vector<TeachDisplay> teachDisplays(std::string& error) {
    std::vector<TeachDisplay> displays;
    const RawResponse raw = requestCore("/teach/status", "GET");
    JsonDocument doc;
    const auto parse = deserializeJson(doc, raw.body);
    if (raw.status < 200 || raw.status >= 300 || parse) {
        error = raw.status <= 0 ? "hub unreachable" : "Could not read hub displays";
        return displays;
    }
    for (JsonObjectConst row : doc["displays"].as<JsonArrayConst>()) {
        TeachDisplay display;
        display.id = row["id"] | "";
        display.name = row["name"] | "Display";
        display.width = row["width"] | 0;
        display.height = row["height"] | 0;
        display.primary = row["primary"] | false;
        displays.push_back(std::move(display));
    }
    return displays;
}

TeachStatus teachStart(const std::string& displayId) {
    JsonDocument doc;
    doc["display_id"] = displayId.empty() ? "primary" : displayId;
    doc["fps"] = 15;
    doc["include_cursor"] = true;
    String body; serializeJson(doc, body);
    return decodeTeach(requestCore("/teach/start", "POST", body));
}

TeachStatus teachMark(const std::string& sessionId, const std::string& note) {
    JsonDocument doc; doc["note"] = note;
    String body; serializeJson(doc, body);
    return decodeTeach(requestCore("/teach/" + sessionId + "/mark", "POST", body));
}

TeachStatus teachStop(const std::string& sessionId) {
    JsonDocument doc; doc["transcribe"] = true;
    String body; serializeJson(doc, body);
    return decodeTeach(requestCore("/teach/" + sessionId + "/stop", "POST", body));
}

TeachStatus teachStatus(const std::string& sessionId) {
    return decodeTeach(requestCore("/teach/" + sessionId, "GET"));
}

}  // namespace host
}  // namespace maz
