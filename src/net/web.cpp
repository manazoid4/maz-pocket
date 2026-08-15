// MAZ Pocket local web control plane.
//
// The browser UI is served by the Cardputer itself at http://mazpocket.local.
// Reading the shell is harmless, but every useful mutation reuses the existing
// MAZ pairing token. The token is typed into the browser and kept only in
// sessionStorage; firmware never renders it back into HTML/JSON.
#include "web.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../core/launcher.h"
#include "../core/settings.h"
#include "../core/sys.h"
#include "../storage/store.h"
#include "mazhost.h"
#include "net.h"

namespace maz {
namespace web {
namespace {

WebServer gServer(80);
bool      gRunning = false;
bool      gMdns    = false;
uint32_t  gReconnectAt = 0;
uint32_t  gMicTestUntil = 0;

enum class Pending : uint8_t { None, Reboot, Launcher };
Pending  gPending = Pending::None;
uint32_t gPendingAt = 0;

bool        gOtaAuthorised = false;
bool        gOtaOk = false;
std::string gOtaError;

class DiscardSink final : public voice::Sink {
public:
    bool open() override { return true; }
    bool write(const int16_t*, size_t) override { return true; }
    bool close() override { return true; }
    const char* name() const override { return "web-test"; }
};
DiscardSink gMicSink;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>MAZ Pocket</title><style>
:root{color-scheme:dark;--bg:#0b0d10;--panel:#14181d;--line:#2a3036;--text:#e6e9ec;--dim:#9aa4ae;--a:#ff7a18;--c:#22d3ee;--ok:#3ddc84;--bad:#ff4d50}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px ui-monospace,SFMono-Regular,Consolas,monospace}main{max-width:900px;margin:auto;padding:18px}header{display:flex;justify-content:space-between;gap:16px;align-items:end;border-bottom:1px solid var(--line);padding-bottom:12px;margin-bottom:14px}h1{font-size:22px;margin:0;letter-spacing:.08em}.mark{color:var(--a)}.muted{color:var(--dim)}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:10px}.card{background:var(--panel);border:1px solid var(--line);border-radius:9px;padding:13px}.card h2{font-size:12px;color:var(--a);letter-spacing:.12em;margin:0 0 11px}.kv{display:grid;grid-template-columns:1fr auto;gap:7px}.ok{color:var(--ok)}.bad{color:var(--bad)}label{display:block;color:var(--dim);margin:9px 0 4px}input,select,button{font:inherit}input,select{width:100%;background:#090b0d;color:var(--text);border:1px solid var(--line);padding:8px;border-radius:6px}button{background:#20252a;color:var(--text);border:1px solid #3a4249;border-radius:6px;padding:8px 10px;cursor:pointer;margin:3px 2px 3px 0}button.primary{background:var(--a);color:#090b0d;border-color:var(--a);font-weight:700}button.warn{border-color:var(--bad)}#unlock{display:flex;gap:7px}#unlock input{flex:1}.meter{height:8px;background:#080a0c;border:1px solid var(--line);margin-top:8px}.meter i{display:block;height:100%;width:0;background:var(--c)}small{color:var(--dim)}pre{white-space:pre-wrap;color:var(--dim);min-height:32px}progress{width:100%}.hidden{display:none}
</style></head><body><main>
<header><div><h1><span class=mark>MAZ</span> POCKET</h1><div class=muted>LOCAL CONTROL PLANE / mazpocket.local</div></div><div id=topState>LOCKED</div></header>
<div class=grid>
<section class=card><h2>UNLOCK</h2><div id=unlock><input id=token type=password placeholder="MAZ pairing token"><button class=primary onclick=unlock()>OPEN</button></div><small>Stored only in this browser tab/session.</small></section>
<section class=card><h2>DEVICE</h2><div class=kv><span>Version</span><b id=version>-</b><span>Battery</span><b id=battery>-</b><span>Uptime</span><b id=uptime>-</b><span>Storage</span><b id=storage>-</b><span>Heap</span><b id=heap>-</b><span>IMU</span><b id=imu>-</b></div></section>
<section class=card><h2>NETWORK</h2><div class=kv><span>Wi-Fi</span><b id=wifi>-</b><span>IP</span><b id=ip>-</b><span>Signal</span><b id=rssi>-</b><span>MAZ Host</span><b id=host>-</b><span>Agents</span><b id=agents>-</b></div></section>
<section class=card><h2>SELF TEST</h2><button onclick="action('speaker')">SPEAKER</button><button onclick="action('mic')">MIC 4 SEC</button><button onclick="action('host')">HOST PROBE</button><div class=meter><i id=micbar></i></div><small id=mictxt>mic idle</small></section>
<section class=card id=configCard><h2>CONFIGURATION</h2><label>Wi-Fi SSID</label><input id=ssid><label>Wi-Fi password</label><input id=pass type=password placeholder="leave blank to keep existing"><label>MAZ Host IP/name</label><input id=hostaddr><label>MAZ Host port</label><input id=hostport type=number value=8787><label>Remote HTTPS fallback</label><input id=remote placeholder="https://...ts.net"><label>Voice route</label><select id=route><option value=0>LOCAL</option><option value=1>AUTO</option><option value=2>CLOUD</option></select><label><input style="width:auto" id=tts type=checkbox> spoken replies</label><br><button class=primary onclick=saveConfig()>SAVE</button><small>Wi-Fi changes reconnect after the response is sent.</small></section>
<section class=card><h2>BROWSER OTA</h2><input id=fw type=file accept=".bin,application/octet-stream"><button class=primary onclick=ota()>UPLOAD FIRMWARE</button><progress id=prog value=0 max=100></progress><small>Authenticated LAN update. M5Launcher/data partitions are preserved.</small></section>
<section class=card><h2>RECOVERY</h2><button onclick="action('reboot')">REBOOT</button><button class=warn onclick="action('launcher')">BACK TO M5LAUNCHER</button><small>Launcher hand-back is refused by firmware if there is nowhere safe to land.</small></section>
<section class=card><h2>LOG</h2><pre id=log>Open with your MAZ token to load live state.</pre></section>
</div></main><script>
let tok=sessionStorage.getItem('mazToken')||''; if(tok)document.getElementById('token').value=tok;
function hdr(){return tok?{'X-MAZ-Token':tok}:{}}
function log(x){document.getElementById('log').textContent=String(x)}
function fmtSec(s){s=Number(s||0);let h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h+'h '+m+'m'}
async function unlock(){tok=document.getElementById('token').value.trim();sessionStorage.setItem('mazToken',tok);await refresh(true)}
async function refresh(fill=false){try{let r=await fetch('/api/status',{headers:hdr()});let d=await r.json();document.getElementById('version').textContent=d.version||'-';if(d.locked){document.getElementById('topState').textContent='LOCKED';return}document.getElementById('topState').innerHTML='<span class=ok>ONLINE</span>';document.getElementById('battery').textContent=(d.battery<0?'?':d.battery+'%')+(d.charging?' / CHG':'');document.getElementById('uptime').textContent=fmtSec(d.uptime);document.getElementById('storage').textContent=d.storage;document.getElementById('heap').textContent=Math.round(d.heap/1024)+' KB';document.getElementById('imu').textContent=d.imu?'READY':'OFF';document.getElementById('wifi').textContent=d.wifi_ssid||'OFF';document.getElementById('ip').textContent=d.ip||'-';document.getElementById('rssi').textContent=d.rssi?d.rssi+' dBm':'-';document.getElementById('host').textContent=d.host_link||'OFFLINE';document.getElementById('agents').textContent=(d.agents_working||0)+' WORK / '+(d.agents_waiting||0)+' WAIT / '+(d.agents_stale||0)+' STALE';document.getElementById('micbar').style.width=Math.round((d.mic_level||0)*100)+'%';document.getElementById('mictxt').textContent=d.mic_test?'mic test live / '+Math.round((d.mic_level||0)*100)+'%':'mic idle';if(fill){document.getElementById('ssid').value=d.wifi_ssid||'';document.getElementById('hostaddr').value=d.host_addr||'';document.getElementById('hostport').value=d.host_port||8787;document.getElementById('remote').value=d.remote_url||'';document.getElementById('route').value=String(d.route||0);document.getElementById('tts').checked=!!d.tts;log('authenticated / live device state loaded')}}catch(e){document.getElementById('topState').innerHTML='<span class=bad>OFFLINE</span>'}}
async function action(name){let r=await fetch('/api/action',{method:'POST',headers:{...hdr(),'Content-Type':'application/json'},body:JSON.stringify({action:name})});let t=await r.text();log(t);if(r.ok)setTimeout(refresh,350)}
async function saveConfig(){let body={wifi_ssid:ssid.value.trim(),host_addr:hostaddr.value.trim(),host_port:Number(hostport.value||8787),remote_url:remote.value.trim(),route:Number(route.value),tts:tts.checked};if(pass.value.length)body.wifi_pass=pass.value;let r=await fetch('/api/config',{method:'POST',headers:{...hdr(),'Content-Type':'application/json'},body:JSON.stringify(body)});log(await r.text());pass.value='';setTimeout(()=>refresh(true),1500)}
async function ota(){let f=document.getElementById('fw').files[0];if(!f){log('choose a firmware .bin first');return}let fd=new FormData();fd.append('firmware',f);let x=new XMLHttpRequest();x.open('POST','/api/ota');x.setRequestHeader('X-MAZ-Token',tok);x.upload.onprogress=e=>{if(e.lengthComputable)document.getElementById('prog').value=e.loaded*100/e.total};x.onload=()=>{log(x.responseText);if(x.status===200)document.getElementById('prog').value=100};x.onerror=()=>log('OTA connection failed');x.send(fd)}
if(tok)refresh(true);else refresh(false);setInterval(()=>refresh(false),2000);
</script></body></html>
)HTML";

bool authorised() {
    if (Cfg.hostToken.empty()) return false;
    return gServer.hasHeader("X-MAZ-Token") &&
           gServer.header("X-MAZ-Token") == Cfg.hostToken.c_str();
}

void jsonError(int code, const char* error) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = error;
    String body;
    serializeJson(doc, body);
    gServer.send(code, "application/json", body);
}

const char* storageName() {
    switch (Sys.storage) {
        case Storage::Internal: return "INTERNAL";
        case Storage::SD: return "SD";
        default: return "NONE";
    }
}

void status() {
    JsonDocument doc;
    doc["ok"] = true;
    doc["version"] = MAZ_POCKET_VERSION;
    if (!authorised()) {
        doc["locked"] = true;
    } else {
        doc["locked"] = false;
        doc["battery"] = Sys.batteryPct;
        doc["charging"] = Sys.charging;
        doc["uptime"] = Sys.uptimeSeconds();
        doc["storage"] = storageName();
        doc["heap"] = ESP.getFreeHeap();
        doc["imu"] = M5.Imu.isEnabled();
        doc["wifi_ssid"] = Sys.wifiSsid;
        doc["ip"] = Sys.ip;
        doc["rssi"] = net::rssi();
        doc["host_addr"] = Cfg.hostAddr;
        doc["host_port"] = Cfg.hostPort;
        doc["remote_url"] = Cfg.hostRemoteUrl;
        doc["host_link"] = host::linkName();
        doc["agents_working"] = Sys.agentsWorking;
        doc["agents_waiting"] = Sys.agentsWaiting;
        doc["agents_stale"] = Sys.agentsStale;
        doc["needs_maz"] = Sys.agentQuestion;
        doc["route"] = Cfg.talkRoute;
        doc["tts"] = Cfg.ttsEnabled;
        doc["recording"] = Sys.recording;
        doc["mic_test"] = gMicTestUntil != 0;
        doc["mic_level"] = voice::level();
        doc["mic_clipped"] = voice::clipped();
    }
    String body;
    serializeJson(doc, body);
    gServer.send(200, "application/json", body);
}

void saveConfig() {
    if (!authorised()) return jsonError(401, "unauthorised");
    JsonDocument doc;
    if (deserializeJson(doc, gServer.arg("plain")))
        return jsonError(400, "invalid_json");

    bool wifiChanged = false;
    if (doc["wifi_ssid"].is<const char*>()) {
        const std::string next = doc["wifi_ssid"].as<const char*>();
        wifiChanged = next != Cfg.wifiSsid;
        Cfg.wifiSsid = next;
    }
    if (doc["wifi_pass"].is<const char*>()) {
        Cfg.wifiPass = doc["wifi_pass"].as<const char*>();
        wifiChanged = true;
    }
    if (doc["host_addr"].is<const char*>())
        Cfg.hostAddr = doc["host_addr"].as<const char*>();
    if (doc["host_port"].is<int>()) {
        const int p = doc["host_port"].as<int>();
        if (p < 1 || p > 65535) return jsonError(400, "invalid_host_port");
        Cfg.hostPort = static_cast<uint16_t>(p);
    }
    if (doc["remote_url"].is<const char*>()) {
        const std::string remote = doc["remote_url"].as<const char*>();
        if (!remote.empty() && remote.rfind("https://", 0) != 0)
            return jsonError(400, "remote_url_requires_https");
        Cfg.hostRemoteUrl = remote;
    }
    if (doc["route"].is<int>()) {
        const int route = doc["route"].as<int>();
        if (route < 0 || route > 2) return jsonError(400, "invalid_route");
        Cfg.talkRoute = static_cast<uint8_t>(route);
    }
    if (doc["tts"].is<bool>()) Cfg.ttsEnabled = doc["tts"].as<bool>();

    Cfg.save();
    Sys.hostAddr = Cfg.hostAddr;
    Sys.hostPort = Cfg.hostPort;
    if (wifiChanged) gReconnectAt = millis() + 900;

    gServer.send(200, "application/json",
                 wifiChanged ? "{\"ok\":true,\"reconnecting\":true}"
                             : "{\"ok\":true}");
}

void action() {
    if (!authorised()) return jsonError(401, "unauthorised");
    JsonDocument doc;
    if (deserializeJson(doc, gServer.arg("plain")))
        return jsonError(400, "invalid_json");
    const std::string name = doc["action"] | "";

    if (name == "speaker") {
        if (voice::state() == voice::State::Listening)
            return jsonError(409, "mic_is_busy");
        sfx::lineOpen();
        gServer.send(200, "application/json", "{\"ok\":true,\"result\":\"speaker_test_played\"}");
        return;
    }
    if (name == "mic") {
        if (voice::state() != voice::State::Idle)
            return jsonError(409, "audio_is_busy");
        if (!voice::start(&gMicSink, 4))
            return jsonError(500, voice::lastError());
        gMicTestUntil = millis() + 4000;
        gServer.send(200, "application/json", "{\"ok\":true,\"result\":\"mic_test_started\"}");
        return;
    }
    if (name == "host") {
        const bool ok = host::health();
        gServer.send(ok ? 200 : 503, "application/json",
                     ok ? "{\"ok\":true,\"result\":\"host_online\"}"
                        : "{\"ok\":false,\"error\":\"host_offline\"}");
        return;
    }
    if (name == "reboot" || name == "launcher") {
        gPending = name == "reboot" ? Pending::Reboot : Pending::Launcher;
        gPendingAt = millis() + 700;
        gServer.send(200, "application/json",
                     name == "reboot" ? "{\"ok\":true,\"result\":\"rebooting\"}"
                                      : "{\"ok\":true,\"result\":\"launcher_handoff_requested\"}");
        return;
    }
    jsonError(400, "action_not_allowed");
}

void otaUpload() {
    HTTPUpload& upload = gServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        gOtaAuthorised = authorised();
        gOtaOk = false;
        gOtaError.clear();
        if (!gOtaAuthorised) return;
        if (voice::state() == voice::State::Listening) voice::stop();
        voice::stopPlayback();
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            gOtaError = "update_begin_failed";
        return;
    }
    if (!gOtaAuthorised || !gOtaError.empty()) return;
    if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            gOtaError = "update_write_failed";
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) gOtaError = "update_verify_failed";
        else gOtaOk = true;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        gOtaError = "upload_aborted";
    }
}

void otaFinish() {
    if (!gOtaAuthorised) return jsonError(401, "unauthorised");
    if (!gOtaOk) return jsonError(500, gOtaError.empty() ? "update_failed" : gOtaError.c_str());
    gPending = Pending::Reboot;
    gPendingAt = millis() + 900;
    gServer.send(200, "application/json", "{\"ok\":true,\"result\":\"firmware_verified_rebooting\"}");
}

void start() {
    if (gRunning || WiFi.status() != WL_CONNECTED) return;
    const char* headers[] = {"X-MAZ-Token"};
    gServer.collectHeaders(headers, 1);
    gServer.on("/", HTTP_GET, []() { gServer.send_P(200, "text/html", PAGE); });
    gServer.on("/api/status", HTTP_GET, status);
    gServer.on("/api/config", HTTP_POST, saveConfig);
    gServer.on("/api/action", HTTP_POST, action);
    gServer.on("/api/ota", HTTP_POST, otaFinish, otaUpload);
    gServer.onNotFound([]() { jsonError(404, "not_found"); });
    gServer.begin();
    gRunning = true;

    if (!gMdns && MDNS.begin("mazpocket")) {
        MDNS.addService("http", "tcp", 80);
        gMdns = true;
    }
    Serial.printf("[web] http://mazpocket.local ip=%s\n",
                  WiFi.localIP().toString().c_str());
}

}  // namespace

void begin() { start(); }

void update() {
    if (WiFi.status() == WL_CONNECTED) {
        start();
        if (gRunning) gServer.handleClient();
    }

    if (gMicTestUntil) {
        if (voice::state() != voice::State::Listening || millis() >= gMicTestUntil) {
            if (voice::state() == voice::State::Listening) voice::stop();
            gMicTestUntil = 0;
        }
    }

    if (gReconnectAt && millis() >= gReconnectAt) {
        gReconnectAt = 0;
        gRunning = false;
        if (gMdns) {
            MDNS.end();
            gMdns = false;
        }
        net::begin();
        return;
    }

    if (gPending != Pending::None && millis() >= gPendingAt) {
        const Pending p = gPending;
        gPending = Pending::None;
        if (p == Pending::Launcher) launcher::reboot();
        else ESP.restart();
    }
}

bool running() { return gRunning && WiFi.status() == WL_CONNECTED; }

}  // namespace web
}  // namespace maz
