#include "mazhost.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "../core/settings.h"
#include "../core/sys.h"

namespace maz {
namespace host {
namespace {

bool coreBegin(HTTPClient& http, const std::string& path) {
    if (Cfg.hostAddr.empty() || Cfg.hostToken.empty() || WiFi.status() != WL_CONNECTED)
        return false;
    const std::string url = "http://" + Cfg.hostAddr + ":" +
                            std::to_string(Cfg.hostPort) + path;
    http.setConnectTimeout(1800);
    http.setTimeout(12000);
    if (!http.begin(url.c_str())) return false;
    http.addHeader("Authorization", ("Bearer " + Cfg.hostToken).c_str());
    return true;
}

std::string detailFrom(JsonDocument& doc, const char* fallback) {
    if (doc["detail"].is<const char*>()) return doc["detail"].as<const char*>();
    if (doc["error"].is<const char*>()) return doc["error"].as<const char*>();
    return fallback;
}

CoreJob decodeJob(const String& body, int status) {
    CoreJob out;
    JsonDocument doc;
    if (status < 200 || status >= 300 || deserializeJson(doc, body)) {
        out.error = "invalid Core job response";
        if (!doc.isNull()) out.error = detailFrom(doc, out.error.c_str());
        return out;
    }
    out.id = doc["id"] | "";
    out.state = doc["state"] | "";
    out.action = doc["action"] | "";
    out.project = doc["project"] | "";
    out.output = doc["output"] | "";
    out.error = doc["error"] | "";
    if (doc["ok"].is<bool>()) out.ok = doc["ok"].as<bool>();
    else out.ok = out.state == "queued" || out.state == "running";
    return out;
}

}  // namespace

CoreStatus coreStatus() {
    CoreStatus out;
    HTTPClient http;
    if (!coreBegin(http, "/core/status")) {
        out.error = "MAZ Core not paired / Wi-Fi offline";
        return out;
    }
    const int status = http.GET();
    const String body = status > 0 ? http.getString() : String();
    http.end();
    if (status <= 0) {
        Sys.hostOnline = false;
        out.error = "MAZ Core unreachable";
        return out;
    }
    JsonDocument doc;
    if (status != 200 || deserializeJson(doc, body)) {
        out.error = "invalid Core response";
        return out;
    }
    Sys.hostOnline = true;
    out.ok = true;
    out.hostname = doc["hostname"] | "PC";
    out.model = doc["ollama_model"] | "";
    out.projects = doc["project_count"] | 0;
    out.ollama = doc["ollama"]["online"] | false;
    return out;
}

std::vector<CoreProject> coreProjects(std::string& error) {
    std::vector<CoreProject> out;
    error.clear();
    HTTPClient http;
    if (!coreBegin(http, "/core/projects")) {
        error = "MAZ Core not paired / Wi-Fi offline";
        return out;
    }
    const int status = http.GET();
    const String body = status > 0 ? http.getString() : String();
    http.end();
    if (status <= 0) {
        Sys.hostOnline = false;
        error = "MAZ Core unreachable";
        return out;
    }
    JsonDocument doc;
    if (status != 200 || deserializeJson(doc, body)) {
        error = "invalid Core response";
        return out;
    }
    Sys.hostOnline = true;
    for (JsonObject item : doc["projects"].as<JsonArray>()) {
        CoreProject p;
        p.name = item["name"] | "project";
        p.branch = item["branch"] | "";
        p.kind = item["kind"] | "";
        p.dirty = item["dirty"] | 0;
        out.push_back(p);
        if (out.size() >= 32) break;
    }
    return out;
}

Reply coreAction(const std::string& action, const std::string& project) {
    Reply out;
    HTTPClient http;
    if (!coreBegin(http, "/core/action")) {
        out.error = "MAZ Core not paired / Wi-Fi offline";
        return out;
    }
    http.addHeader("Content-Type", "application/json");
    JsonDocument request;
    request["action"] = action;
    request["project"] = project;
    String payload;
    serializeJson(request, payload);
    const int status = http.POST(payload);
    const String body = status > 0 ? http.getString() : String();
    http.end();
    out.status = status;
    if (status <= 0) {
        Sys.hostOnline = false;
        out.error = "MAZ Core unreachable";
        return out;
    }
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        out.error = "invalid Core response";
        return out;
    }
    if (status < 200 || status >= 300) {
        out.error = detailFrom(doc, "Core action failed");
        return out;
    }
    Sys.hostOnline = true;
    out.ok = doc["ok"] | false;
    out.text = doc["output"] | "";
    if (out.text.empty()) out.text = out.ok ? "Action complete" : "Action failed";
    if (!out.ok) out.error = out.text;
    return out;
}

CoreJob coreStartJob(const std::string& action, const std::string& project) {
    CoreJob out;
    HTTPClient http;
    if (!coreBegin(http, "/core/job")) {
        out.error = "MAZ Core not paired / Wi-Fi offline";
        return out;
    }
    http.addHeader("Content-Type", "application/json");
    JsonDocument request;
    request["action"] = action;
    request["project"] = project;
    String payload;
    serializeJson(request, payload);
    const int status = http.POST(payload);
    const String body = status > 0 ? http.getString() : String();
    http.end();
    if (status <= 0) {
        Sys.hostOnline = false;
        out.error = "MAZ Core unreachable";
        return out;
    }
    Sys.hostOnline = status >= 200 && status < 300;
    return decodeJob(body, status);
}

CoreJob coreJob(const std::string& id) {
    CoreJob out;
    if (id.empty()) { out.error = "job id missing"; return out; }
    HTTPClient http;
    if (!coreBegin(http, "/core/job/" + id)) {
        out.error = "MAZ Core not paired / Wi-Fi offline";
        return out;
    }
    const int status = http.GET();
    const String body = status > 0 ? http.getString() : String();
    http.end();
    if (status <= 0) {
        Sys.hostOnline = false;
        out.error = "MAZ Core unreachable";
        return out;
    }
    Sys.hostOnline = status >= 200 && status < 300;
    return decodeJob(body, status);
}

}  // namespace host
}  // namespace maz
