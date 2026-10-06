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

static CoreInfo gCoreInfo;
const CoreInfo& coreInfo() { return gCoreInfo; }
void setCoreInfo(const CoreInfo& info) { gCoreInfo = info; }
bool updateReady() {
    return gCoreInfo.ok && !gCoreInfo.fwVersion.empty() &&
           (gCoreInfo.fwVersion != NOD_FW_VERSION || gCoreInfo.fwSha != NOD_FW_SHA);
}

CoreInfo fetchCoreInfo() {
    CoreInfo out;
    HTTPClient http;
    if (!coreBegin(http, "/health")) return out;
    const int status = http.GET();
    const String body = status > 0 ? http.getString() : String();
    http.end();
    JsonDocument doc;
    if (status != 200 || deserializeJson(doc, body)) return out;
    out.ok = true;
    out.version = doc["version"] | "";
    out.fwVersion = doc["fw_latest"]["version"] | "";
    out.fwSha = doc["fw_latest"]["sha"] | "";
    return out;
}

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

// ---- Wi-Fi firmware update (U2) ---------------------------------------------------------------
#include <M5Unified.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

namespace maz {
namespace host {
namespace {

void fwProgress(const char* line, int pct) {
    auto& d = M5.Display;
    d.fillRect(0, 50, 240, 40, 0);
    d.setTextColor(0xFFFF, 0);
    d.setTextDatum(top_left);
    d.drawString(line, 8, 52);
    d.drawRect(8, 72, 224, 10, 0xFFFF);
    d.fillRect(8, 72, 224 * pct / 100, 10, 0x07E0);
}

bool fwFail(const char* why) {
    Serial.printf("[fw] FAIL %s\n", why);
    fwProgress(why, 0);
    delay(2500);
    return false;
}

}  // namespace

bool fwUpdate() {
    HTTPClient m;
    if (!coreBegin(m, "/fw/manifest")) return fwFail("Core offline");
    const int ms = m.GET();
    const String mb = ms > 0 ? m.getString() : String();
    m.end();
    JsonDocument doc;
    if (ms != 200 || deserializeJson(doc, mb)) return fwFail("no firmware on Core");
    const size_t size = doc["size"] | 0;
    const String want = doc["sha256"] | "";
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (!target || size < 65536 || size > target->size || want.length() != 64) return fwFail("bad manifest");

    HTTPClient http;
    if (!coreBegin(http, "/fw/latest.bin")) return fwFail("Core offline");
    http.setTimeout(20000);
    if (http.GET() != 200) { http.end(); return fwFail("download refused"); }
    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(target, size, &ota) != ESP_OK) { http.end(); return fwFail("OTA begin failed"); }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    static uint8_t buf[2048];
    WiFiClient* stream = http.getStreamPtr();
    size_t done = 0;
    uint32_t last = millis(), shown = 0;
    while (done < size) {
        const size_t n = stream->readBytes(buf, min(sizeof(buf), size - done));
        if (n == 0) {
            if (millis() - last > 15000) { esp_ota_abort(ota); http.end(); return fwFail("download stalled"); }
            delay(2);
            continue;
        }
        last = millis();
        if (esp_ota_write(ota, buf, n) != ESP_OK) { esp_ota_abort(ota); http.end(); return fwFail("flash write failed"); }
        mbedtls_sha256_update(&sha, buf, n);
        done += n;
        if (millis() - shown > 400) { shown = millis(); fwProgress("Updating nod...", static_cast<int>(done * 100 / size)); }
    }
    http.end();
    uint8_t out[32];
    mbedtls_sha256_finish(&sha, out);
    mbedtls_sha256_free(&sha);
    char hex[65];
    for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", out[i]);
    if (!want.equalsIgnoreCase(hex)) { esp_ota_abort(ota); return fwFail("checksum mismatch"); }
    if (esp_ota_end(ota) != ESP_OK) return fwFail("image invalid");

    Preferences p;
    p.begin("nodfw", false);
    p.putString("prev", esp_ota_get_running_partition()->label);
    p.putInt("boots", 0);
    p.end();
    if (esp_ota_set_boot_partition(target) != ESP_OK) return fwFail("cannot set boot slot");
    fwProgress("Updated - rebooting", 100);
    Serial.printf("[fw] OK %u bytes -> %s\n", (unsigned)size, target->label);
    delay(600);
    ESP.restart();
    return true;
}

void fwBootGuard() {
    Preferences p;
    p.begin("nodfw", false);
    if (p.isKey("prev")) {
        const int boots = p.getInt("boots", 0) + 1;
        p.putInt("boots", boots);
        if (boots > 3) {  // fresh update restarted 3 times without ever running stable: go back
            const String prev = p.getString("prev", "");
            const esp_partition_t* back = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, prev.c_str());
            p.remove("prev");
            p.end();
            if (back && esp_ota_set_boot_partition(back) == ESP_OK) { Serial.println("[fw] rollback"); ESP.restart(); }
            return;
        }
    }
    p.end();
}

void fwMarkGood() {
    Preferences p;
    p.begin("nodfw", false);
    if (p.isKey("prev")) { p.remove("prev"); p.remove("boots"); Serial.println("[fw] marked good"); }
    p.end();
}

}  // namespace host
}  // namespace maz
