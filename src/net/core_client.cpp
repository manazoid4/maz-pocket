#include "mazhost.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "remote_tls.h"
#include <WiFi.h>

#include "../core/settings.h"
#include "../core/sys.h"

namespace maz {
namespace host {
namespace {

// Follows the link chosen by host::health(): LAN is always tried first (short
// timeout) there, and the remote HTTPS URL is used only while LAN is down.
bool coreBegin(NodHttp& http, const std::string& path, bool allowRemote = true) {
    if (Cfg.hostToken.empty() || WiFi.status() != WL_CONNECTED) return false;
    bool remote = false;
    std::string base;
    if (allowRemote && onRemoteLink() && Cfg.hostRemoteUrl.rfind("https://", 0) == 0) {
        base = Cfg.hostRemoteUrl;
        while (!base.empty() && base.back() == '/') base.pop_back();
        remote = true;
    } else if (!Cfg.hostAddr.empty()) {
        base = "http://" + Cfg.hostAddr + ":" + std::to_string(Cfg.hostPort);
    } else {
        return false;
    }
    http.setConnectTimeout(remote ? 6500 : 1800);
    http.setTimeout(12000);
    if (!http.open((base + path).c_str(), remote)) return false;
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
    NodHttp http;
    const bool viaRemote = onRemoteLink();
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
    // Core tells us its Tailscale Funnel URL. Accept only when read over the LAN
    // (not via the remote link), https, a *.ts.net host, bounded length.
    if (!viaRemote) {
        std::string ru = doc["remote_url"] | "";
        while (!ru.empty() && ru.back() == '/') ru.pop_back();
        static const char kHttps[] = "https://";
        static const char kSuffix[] = ".ts.net";
        const size_t hostLen = ru.size() > 8 ? ru.size() - 8 : 0;
        bool ok = ru.size() <= 96 && ru.rfind(kHttps, 0) == 0 && hostLen > 7 &&
                  ru.compare(ru.size() - 7, 7, kSuffix) == 0;
        for (size_t i = 8; ok && i < ru.size(); i++) {
            const char c = ru[i];
            ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
        }
        if (ok && ru != Cfg.hostRemoteUrl) {
            Cfg.hostRemoteUrl = ru;
            Cfg.save();
        }
        if (ok) out.remoteUrl = ru;
    }
    return out;
}

BuddyPoll buddyPoll() {
    BuddyPoll out;
    NodHttp http;
    if (!coreBegin(http, "/buddy/summary")) return out;
    const int status = http.GET();
    const String body = status > 0 ? http.getString() : String();
    http.end();
    JsonDocument doc;
    if (status != 200 || deserializeJson(doc, body)) return out;
    out.ok = true;
    out.agent = doc["agent"] | "idle";
    out.count = doc["count"] | 0;
    for (JsonObject it : doc["items"].as<JsonArray>()) {
        BuddyItem b;
        b.id = it["id"] | "";
        b.tool = it["tool"] | "?";
        b.summary = it["summary"] | "";
        b.project = it["project"] | "";
        b.session = it["session_id"] | "";
        b.left = it["left"] | 0;
        if (!b.id.empty()) out.items.push_back(std::move(b));
    }
    return out;
}

bool buddyDecide(const std::string& id, const std::string& decision, std::string& applied) {
    applied.clear();
    NodHttp http;
    if (!coreBegin(http, "/buddy/decide")) return false;
    http.addHeader("Content-Type", "application/json");
    JsonDocument req;
    req["id"] = id;
    req["decision"] = decision;
    String payload;
    serializeJson(req, payload);
    const int status = http.POST(payload);
    const String body = status > 0 ? http.getString() : String();
    http.end();
    if (status == 404) { applied = "gone"; return true; }  // expired or cancelled at the terminal
    JsonDocument doc;
    if (status != 200 || deserializeJson(doc, body)) return false;
    applied = doc["decision"] | "";
    return true;
}

CoreStatus coreStatus() {
    CoreStatus out;
    NodHttp http;
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
    NodHttp http;
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
    NodHttp http;
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
    NodHttp http;
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
    NodHttp http;
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
    NodHttp m;
    if (!coreBegin(m, "/fw/manifest", false)) return fwFail("Update needs home Wi-Fi");
    const int ms = m.GET();
    const String mb = ms > 0 ? m.getString() : String();
    m.end();
    JsonDocument doc;
    if (ms != 200 || deserializeJson(doc, mb)) return fwFail("no firmware on Core");
    const size_t size = doc["size"] | 0;
    const String want = doc["sha256"] | "";
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (!target || size < 65536 || size > target->size || want.length() != 64) return fwFail("bad manifest");

    NodHttp http;
    if (!coreBegin(http, "/fw/latest.bin", false)) return fwFail("Update needs home Wi-Fi");
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
