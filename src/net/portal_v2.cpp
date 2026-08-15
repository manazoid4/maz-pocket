#include "portal.h"

#include <ESPmDNS.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "../audio/sfx.h"
#include "../core/launcher.h"
#include "../core/settings.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../storage/store.h"
#include "host_worker.h"
#include "mazhost.h"
#include "net.h"

namespace maz {
namespace portal {
namespace {

constexpr uint16_t PORT = 80;
constexpr size_t MAX_BODY = 1800;
constexpr size_t MAX_LINE = 320;
constexpr size_t MAX_HEADERS = 1800;
constexpr size_t RX_BUDGET_PER_TICK = 512;
constexpr size_t TX_BUDGET_PER_TICK = 1400;
constexpr uint32_t REQUEST_IDLE_MS = 1400;
constexpr uint32_t REQUEST_MAX_AGE_MS = 5000;
constexpr uint32_t TX_IDLE_MS = 2500;
constexpr size_t SCREEN_PIXELS = 240u * 135u;
constexpr size_t SCREEN_BYTES = SCREEN_PIXELS * 2u;

WiFiServer gServer(PORT);
WiFiClient gClient;
bool gRunning = false;
bool gMdns = false;
uint32_t gReconnectAt = 0;
uint32_t gRebootAt = 0;
bool gToLauncher = false;

enum class RxState : uint8_t { RequestLine, Headers, Body, Ready };
enum class TxKind : uint8_t { None, Text, Page, Screen };

RxState gRxState = RxState::RequestLine;
String gLine;
String gMethod;
String gPath;
String gToken;
String gBody;
size_t gContentLength = 0;
size_t gHeaderBytes = 0;
uint32_t gOpenedAt = 0;
uint32_t gLastRxAt = 0;

TxKind gTxKind = TxKind::None;
String gTxHead;
String gTxText;
size_t gTxHeadAt = 0;
size_t gTxPayloadAt = 0;
uint32_t gLastTxAt = 0;
uint8_t gScreenScratch[TX_BUDGET_PER_TICK];

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name="theme-color" content="#090b0e"><meta name="robots" content="noindex"><title>MAZ Pocket</title><style>
:root{color-scheme:dark;--b:#090b0e;--p:#11161b;--p2:#0d1115;--l:#28313a;--t:#eef2f3;--d:#99a5ad;--a:#ff7a18;--g:#42d58a;--w:#ffc04d;--r:#ff676d}*{box-sizing:border-box}body{margin:0;background:var(--b);color:var(--t);font:13px ui-monospace,SFMono-Regular,Consolas,monospace}main{max-width:920px;margin:auto;padding:12px}.top{position:sticky;top:0;z-index:3;background:#090b0ef2;border-bottom:1px solid var(--l);display:flex;justify-content:space-between;gap:10px;padding:10px 0}.brand{font-size:19px;font-weight:900}.brand b{color:var(--a)}.pills{display:flex;gap:5px;flex-wrap:wrap;justify-content:flex-end}.pill{border:1px solid var(--l);border-radius:999px;padding:4px 7px;color:var(--d);font-size:10px}.pill.ok{color:var(--g)}.pill.bad{color:var(--r)}h1{font-size:23px;margin:18px 0 5px}p{color:var(--d);line-height:1.45}.sec{margin-top:15px}.sec>h2{font-size:10px;letter-spacing:.15em;color:var(--d)}.surfaces{display:grid;grid-template-columns:repeat(3,1fr);gap:7px}.surface,.card{border:1px solid var(--l);border-radius:9px;background:var(--p);padding:11px}.surface{min-height:95px;display:flex;flex-direction:column;justify-content:space-between}.surface strong{font-size:15px}.surface small{display:block;color:var(--d);margin:5px 0 9px;line-height:1.35}.grid{display:grid;grid-template-columns:1fr 1fr;gap:7px}.kv{display:grid;grid-template-columns:1fr auto;gap:6px 10px}.kv span{color:var(--d)}button,input,select{font:inherit;border:1px solid var(--l);border-radius:7px;background:#080b0e;color:var(--t);padding:8px}button{cursor:pointer}button.primary{background:var(--a);border-color:var(--a);color:#08090a;font-weight:900}.row{display:flex;gap:6px;flex-wrap:wrap}.row button{flex:1;min-width:95px}.cfg{display:grid;grid-template-columns:1fr 1fr;gap:6px}.cfg>*{width:100%}.wide{grid-column:1/-1}.status{background:var(--p2);border-radius:7px;margin-top:7px;padding:8px;color:var(--d);white-space:pre-wrap}.badtext{color:var(--r)}.good{color:var(--g)}canvas{width:min(100%,480px);height:auto;image-rendering:pixelated;background:#000;border:1px solid var(--l);border-radius:7px}.note{font-size:11px;color:var(--d)}@media(max-width:650px){.surfaces{grid-template-columns:1fr 1fr}.grid{grid-template-columns:1fr}}@media(max-width:360px){.surface{padding:9px}}
</style></head><body><main><div class="top"><div class="brand"><b>MAZ</b> POCKET <span id="ver" class="note">v-</span></div><div class="pills"><span id="pw" class="pill">Wi-Fi</span><span id="pcore" class="pill">Core</span><span id="pa" class="pill">Agents</span><span id="pb" class="pill">Battery</span></div></div>
<h1>Pocket control surface</h1><p>Fast local controls. Network work stays off the Cardputer UI task; M5Launcher still owns firmware installation.</p>
<div class="sec"><h2>SURFACES</h2><div class="surfaces">
<div class="surface"><div><strong>COMM</strong><small>Voice assistant + safe PC controls.</small></div><button onclick="openApp('talk')">OPEN</button></div><div class="surface"><div><strong>CAPTURE</strong><small>Voice notes + BrainDump.</small></div><button onclick="openApp('braindump')">OPEN</button></div><div class="surface"><div><strong>OPS</strong><small>Agent evidence + nudges.</small></div><button onclick="openApp('nudge')">OPEN</button></div><div class="surface"><div><strong>CONTROL</strong><small>Wi-Fi, Core and device tools.</small></div><button onclick="openApp('desk')">OPEN</button></div><div class="surface"><div><strong>RECALL</strong><small>Inbox, notes and snippets.</small></div><button onclick="openApp('recall')">OPEN</button></div><div class="surface"><div><strong>FLOW</strong><small>Reminders, focus and tasks.</small></div><button onclick="openApp('flow')">OPEN</button></div>
</div></div>
<div class="sec"><h2>STATUS</h2><div class="grid"><div class="card"><div class="kv"><span>App</span><b id="app">-</b><span>Wi-Fi</span><b id="wifi">-</b><span>IP</span><b id="ip">-</b><span>Storage</span><b id="storage">-</b><span>Heap</span><b id="heap">-</b><span>Loop max</span><b id="loop">-</b></div></div><div class="card"><div class="kv"><span>MAZ Core</span><b id="core">-</b><span>Host worker</span><b id="worker">-</b><span>Working agents</span><b id="working">0</b><span>Waiting</span><b id="waiting">0</b><span>Stale</span><b id="stale">0</b></div><div class="row" style="margin-top:9px"><button onclick="act('host')">CORE STATUS</button><button onclick="act('reconnect')">RECONNECT</button></div></div></div></div>
<div class="sec"><h2>LIVE LCD</h2><div class="card"><canvas id="screen" width="240" height="135"></canvas><p class="note">Unlock controls to view the LCD. Frame reads reuse the existing 16-bit shell canvas; no second framebuffer is allocated.</p></div></div>
<div class="sec"><h2>PC QUICK CONTROL</h2><div class="card"><div class="row"><button onclick="pc('desktop')">DESKTOP</button><button onclick="pc('play_pause')">PLAY/PAUSE</button><button onclick="pc('mute')">MUTE</button><button onclick="pc('volume_down')">VOL -</button><button onclick="pc('volume_up')">VOL +</button><button onclick="pc('previous_track')">PREV</button><button onclick="pc('next_track')">NEXT</button><button onclick="pc('lock')">LOCK</button></div><p class="note">Commands are queued on the single bounded Host worker; the browser never waits on MAZ Core HTTP.</p></div></div>
<div class="sec"><h2>UNLOCK + CONFIG</h2><div class="grid"><div class="card"><p class="note">Use the same pairing token as MAZ Core. It stays in this browser session.</p><div class="cfg"><input class="wide" id="token" type="password" placeholder="MAZ pairing token"><button class="primary wide" onclick="unlock()">UNLOCK</button></div><div id="lock" class="status">Controls locked.</div></div><div class="card"><div class="cfg"><input id="hostAddr" placeholder="MAZ Core IP/name"><input id="hostPort" type="number" value="8787"><input class="wide" id="remote" placeholder="Optional private https://..."><select id="route"><option value="0">LOCAL</option><option value="1">AUTO</option><option value="2">CLOUD</option></select><select id="tts"><option value="1">Spoken replies ON</option><option value="0">Spoken replies OFF</option></select><input id="ssid2" placeholder="Backup Wi-Fi SSID"><input id="pass2" type="password" placeholder="Backup Wi-Fi password"><button class="primary wide" onclick="saveCfg()">SAVE</button></div></div></div></div>
<div class="sec"><h2>DEVICE</h2><div class="card"><div class="row"><button onclick="act('speaker')">SPEAKER</button><button onclick="act('storage')">SD CHECK</button><button onclick="act('reboot')">REBOOT</button><button onclick="act('launcher')">M5LAUNCHER</button></div><p class="note">Firmware updates remain app-only M5Launcher installs. No generic OTA partition writer is exposed here.</p><div id="log" class="status">Ready.</div></div></div>
<script>
const $=x=>document.getElementById(x);let token=sessionStorage.getItem('mazToken')||'';if(token)$('token').value=token;const H=()=>token?{'X-MAZ-Token':token}:{};function log(x,b=false){$('log').textContent=x;$('log').className='status '+(b?'badtext':'')}function pill(id,ok,t){let e=$(id);e.textContent=t;e.className='pill '+(ok?'ok':'bad')}async function status(fill=false){try{let r=await fetch('/api/status',{headers:H(),cache:'no-store'}),d=await r.json();$('ver').textContent='v'+d.version;$('app').textContent=d.app;$('wifi').textContent=d.wifi_ssid||'offline';$('ip').textContent=d.ip||'-';$('storage').textContent=d.storage;$('heap').textContent=Math.round((d.free_heap||0)/1024)+'K';$('loop').textContent=(d.loop_max||0)+'ms';$('core').textContent=d.host_link||'OFFLINE';$('worker').textContent=d.host_worker||'IDLE';$('working').textContent=d.agents_working||0;$('waiting').textContent=d.agents_waiting||0;$('stale').textContent=d.agents_stale||0;pill('pw',!!d.wifi_connected,d.wifi_connected?'Wi-Fi ✓':'Wi-Fi ×');pill('pcore',d.host_link&&d.host_link!='OFFLINE',d.host_link&&d.host_link!='OFFLINE'?'Core ✓':'Core ×');pill('pa',!(d.agents_stale||d.agent_question),d.agent_question?'Needs Maz':(d.agents_stale?d.agents_stale+' stale':'Agents ✓'));pill('pb',(d.battery||0)>15,d.battery<0?'Battery ?':'Battery '+d.battery+'%');$('lock').innerHTML=d.unlocked?'<span class="good">CONTROLS UNLOCKED</span>':'Controls locked.';if(fill&&d.unlocked){$('hostAddr').value=d.host_addr||'';$('hostPort').value=d.host_port||8787;$('remote').value=d.remote_url||'';$('route').value=String(d.route||0);$('tts').value=d.tts?'1':'0';$('ssid2').value=d.ssid2||''}}catch(e){pill('pw',false,'Device offline');log('Lost connection to MAZ Pocket.',true)}}async function unlock(){token=$('token').value.trim();sessionStorage.setItem('mazToken',token);await status(true);await frame()}async function post(path,body){try{let r=await fetch(path,{method:'POST',headers:{...H(),'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)}),t=await r.text();log(t,!r.ok);setTimeout(()=>status(false),200);return r.ok?t:null}catch(e){log('Request failed.',true);return null}}function act(a){return post('/api/action',{action:a})}function openApp(a){return act('open:'+a)}function pc(a){return act('pc:'+a)}async function saveCfg(){let x=await post('/api/config',{host_addr:$('hostAddr').value.trim(),host_port:$('hostPort').value||'8787',remote_url:$('remote').value.trim(),route:$('route').value,tts:$('tts').value,ssid2:$('ssid2').value.trim(),pass2:$('pass2').value});if(x)setTimeout(()=>status(true),300)}async function frame(){if(!token)return;try{let r=await fetch('/api/screen',{headers:H(),cache:'no-store'});if(!r.ok)return;let b=new Uint8Array(await r.arrayBuffer());if(b.length!=64800)return;let c=$('screen').getContext('2d'),im=c.createImageData(240,135);for(let p=0,i=0;p<32400;p++,i+=2){let v=b[i]|b[i+1]<<8,o=p*4;im.data[o]=((v>>11)&31)*255/31;im.data[o+1]=((v>>5)&63)*255/63;im.data[o+2]=(v&31)*255/31;im.data[o+3]=255}c.putImageData(im,0,0)}catch(e){}}status(true);setInterval(()=>status(false),2500);setInterval(()=>frame(),1000);
</script></main></body></html>)HTML";

String jsonEscape(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); ++i) {
        const char c = in[i];
        if (c == '\\' || c == '"') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (static_cast<uint8_t>(c) >= 0x20) out += c;
    }
    return out;
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

String decodeForm(String value) {
    value.replace('+', ' ');
    String out;
    out.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%' && i + 2 < value.length()) {
            const int a = hexNibble(value[i + 1]);
            const int b = hexNibble(value[i + 2]);
            if (a >= 0 && b >= 0) {
                out += static_cast<char>((a << 4) | b);
                i += 2;
                continue;
            }
        }
        out += value[i];
    }
    return out;
}

String formValue(const String& body, const char* key) {
    const String needle = String(key) + '=';
    int pos = 0;
    while (pos < static_cast<int>(body.length())) {
        const int end = body.indexOf('&', pos);
        const String field = body.substring(pos, end < 0 ? body.length() : end);
        if (field.startsWith(needle)) return decodeForm(field.substring(needle.length()));
        if (end < 0) break;
        pos = end + 1;
    }
    return "";
}

bool tokenOk(const String& token) {
    return !Cfg.hostToken.empty() && token == Cfg.hostToken.c_str();
}

const char* reason(int code) {
    switch (code) {
        case 200: return "OK";
        case 202: return "Accepted";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 408: return "Request Timeout";
        case 429: return "Too Many Requests";
        default: return "Error";
    }
}

String makeHead(int code, const char* type, size_t len) {
    String h;
    h.reserve(180);
    h += "HTTP/1.1 "; h += String(code); h += ' '; h += reason(code); h += "\r\n";
    h += "Content-Type: "; h += type;
    h += "\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ";
    h += String(len);
    h += "\r\nX-Content-Type-Options: nosniff\r\n\r\n";
    return h;
}

void resetRx() {
    gRxState = RxState::RequestLine;
    gLine = "";
    gMethod = "";
    gPath = "";
    gToken = "";
    gBody = "";
    gContentLength = 0;
    gHeaderBytes = 0;
}

void resetTx() {
    gTxKind = TxKind::None;
    gTxHead = "";
    gTxText = "";
    gTxHeadAt = 0;
    gTxPayloadAt = 0;
}

void closeClient() {
    if (gClient) gClient.stop();
    resetRx();
    resetTx();
}

void queueText(int code, const char* type, const String& body) {
    gTxText = body;
    gTxHead = makeHead(code, type, gTxText.length());
    gTxHeadAt = 0;
    gTxPayloadAt = 0;
    gTxKind = TxKind::Text;
    gLastTxAt = millis();
}

void queuePage() {
    gTxHead = makeHead(200, "text/html; charset=utf-8", strlen(PAGE));
    gTxHeadAt = 0;
    gTxPayloadAt = 0;
    gTxKind = TxKind::Page;
    gLastTxAt = millis();
}

void queueScreen() {
    gTxHead = makeHead(200, "application/octet-stream", SCREEN_BYTES);
    gTxHead += "";
    gTxHeadAt = 0;
    gTxPayloadAt = 0;
    gTxKind = TxKind::Screen;
    gLastTxAt = millis();
}

String statusJson(bool unlocked) {
    String j;
    j.reserve(1100);
    j += "{\"ok\":true,\"version\":\"" MAZ_POCKET_VERSION "\"";
    j += ",\"unlocked\":"; j += unlocked ? "true" : "false";
    j += ",\"app\":\"" + jsonEscape(shell::currentId()) + "\"";
    j += ",\"battery\":" + String(Sys.batteryPct);
    j += ",\"charging\":"; j += Sys.charging ? "true" : "false";
    j += ",\"storage\":\"" + String(store::backendName()) + "\"";
    j += ",\"sd_present\":"; j += Sys.sdPresent ? "true" : "false";
    j += ",\"sd_unreadable\":"; j += Sys.sdUnreadable ? "true" : "false";
    j += ",\"wifi_connected\":"; j += net::connected() ? "true" : "false";
    j += ",\"wifi_ssid\":\"" + jsonEscape(Sys.wifiSsid.c_str()) + "\"";
    j += ",\"ip\":\"" + jsonEscape(net::ip().c_str()) + "\"";
    j += ",\"rssi\":" + String(net::rssi());
    j += ",\"host_link\":\"" + String(host::linkName()) + "\"";
    j += ",\"host_worker\":\"" + String(host_worker::stateName()) + "\"";
    j += ",\"host_worker_stack\":" + String(host_worker::stackHighWaterBytes());
    j += ",\"agents_working\":" + String(Sys.agentsWorking);
    j += ",\"agents_waiting\":" + String(Sys.agentsWaiting);
    j += ",\"agents_stale\":" + String(Sys.agentsStale);
    j += ",\"agent_question\":"; j += Sys.agentQuestion ? "true" : "false";
    j += ",\"route\":" + String(Cfg.talkRoute);
    j += ",\"tts\":"; j += Cfg.ttsEnabled ? "true" : "false";
    j += ",\"free_heap\":" + String(Sys.freeHeap);
    j += ",\"min_heap\":" + String(Sys.minFreeHeap);
    j += ",\"largest_block\":" + String(Sys.largestFreeBlock);
    j += ",\"loop_max\":" + String(Sys.loopMaxMs);
    j += ",\"screen_bytes\":" + String(SCREEN_BYTES);
    if (unlocked) {
        j += ",\"host_addr\":\"" + jsonEscape(Cfg.hostAddr.c_str()) + "\"";
        j += ",\"host_port\":" + String(Cfg.hostPort);
        j += ",\"remote_url\":\"" + jsonEscape(Cfg.hostRemoteUrl.c_str()) + "\"";
        j += ",\"ssid2\":\"" + jsonEscape(Cfg.wifiSsid2.c_str()) + "\"";
    }
    j += '}';
    return j;
}

bool allowedApp(const String& id) {
    return id == "talk" || id == "braindump" || id == "nudge" ||
           id == "desk" || id == "recall" || id == "flow" ||
           id == "wifi" || id == "tools" || id == "settings" ||
           id == "core" || id == "control";
}

bool allowedPc(const String& id) {
    return id == "desktop" || id == "play_pause" || id == "mute" ||
           id == "volume_down" || id == "volume_up" ||
           id == "previous_track" || id == "next_track" || id == "lock";
}

struct ActionResult {
    int code = 200;
    String text;
};

ActionResult runAction(const String& body) {
    const String action = formValue(body, "action");
    if (action == "reconnect") {
        gReconnectAt = millis() + 150;
        return {202, "Wi-Fi reconnect queued."};
    }
    if (action == "host") {
        return {200, Sys.hostOnline ? String("MAZ Core last known online via ") + host::linkName()
                                    : "MAZ Core is currently marked offline."};
    }
    if (action == "speaker") {
        sfx::lineOpen();
        return {200, "Speaker test played."};
    }
    if (action == "storage") {
        if (Sys.storage != Storage::SD) return {200, "SD is not the active storage backend."};
        const std::string path = "/maz/cache/.portal-check";
        const std::string probe = "MAZ-PORTAL-CHECK";
        if (!store::writeText(path, probe)) return {400, "SD write failed."};
        const bool ok = store::readText(path, 64) == probe;
        store::remove(path);
        return {ok ? 200 : 400, ok ? "SD read/write check passed." : "SD verification failed."};
    }
    if (action == "reboot") {
        gToLauncher = false;
        gRebootAt = millis() + 500;
        return {202, "Rebooting MAZ Pocket..."};
    }
    if (action == "launcher") {
        gToLauncher = true;
        gRebootAt = millis() + 500;
        return {202, "Returning to M5Launcher..."};
    }
    if (action.startsWith("open:")) {
        const String id = action.substring(5);
        if (!allowedApp(id)) return {400, "Unsupported app."};
        shell::goHome();
        if (!shell::pushById(id.c_str())) return {400, "Could not open app."};
        shell::wake();
        return {200, String("Opened ") + id + " on the Cardputer."};
    }
    if (action.startsWith("pc:")) {
        const String id = action.substring(3);
        if (!allowedPc(id)) return {400, "Unsupported PC action."};
        if (!host_worker::submitPcAction(id.c_str(), false))
            return {429, String("MAZ Core worker unavailable: ") + host_worker::stateName()};
        return {202, String("PC action queued: ") + id};
    }
    return {400, "Unknown action."};
}

String saveConfig(const String& body) {
    const String hostAddr = formValue(body, "host_addr");
    const String hostPort = formValue(body, "host_port");
    const String remote = formValue(body, "remote_url");
    const String route = formValue(body, "route");
    const String tts = formValue(body, "tts");
    const String ssid2 = formValue(body, "ssid2");
    const String pass2 = formValue(body, "pass2");

    Cfg.hostAddr = hostAddr.c_str();
    const long parsedPort = hostPort.toInt();
    if (parsedPort > 0 && parsedPort <= 65535) Cfg.hostPort = static_cast<uint16_t>(parsedPort);
    Cfg.hostRemoteUrl = remote.c_str();
    const int parsedRoute = route.toInt();
    Cfg.talkRoute = static_cast<uint8_t>(parsedRoute < 0 ? 0 : (parsedRoute > 2 ? 2 : parsedRoute));
    Cfg.ttsEnabled = tts == "1";
    Cfg.wifiSsid2 = ssid2.c_str();
    if (!pass2.isEmpty()) Cfg.wifiPass2 = pass2.c_str();
    Cfg.save();
    Sys.hostAddr = Cfg.hostAddr;
    Sys.hostPort = Cfg.hostPort;
    return "Configuration saved.";
}

void dispatchRequest() {
    const bool unlocked = tokenOk(gToken);
    if (gMethod == "GET" && (gPath == "/" || gPath == "/index.html")) {
        queuePage();
    } else if (gMethod == "GET" && gPath == "/api/status") {
        queueText(200, "application/json", statusJson(unlocked));
    } else if (gMethod == "GET" && gPath == "/api/screen") {
        if (!unlocked) queueText(401, "text/plain", "Pairing token required for LCD access.");
        else queueScreen();
    } else if (gMethod == "POST" && gPath == "/api/action") {
        if (!unlocked) queueText(401, "text/plain", "Unlock controls with the MAZ pairing token first.");
        else {
            const ActionResult result = runAction(gBody);
            queueText(result.code, "text/plain", result.text);
        }
    } else if (gMethod == "POST" && gPath == "/api/config") {
        if (!unlocked) queueText(401, "text/plain", "Unlock controls with the MAZ pairing token first.");
        else queueText(200, "text/plain", saveConfig(gBody));
    } else {
        queueText(404, "text/plain", "Not found.");
    }
    gRxState = RxState::Ready;
}

bool parseRequestLine(const String& line) {
    const int p1 = line.indexOf(' ');
    const int p2 = line.indexOf(' ', p1 + 1);
    if (p1 <= 0 || p2 <= p1 + 1) return false;
    gMethod = line.substring(0, p1);
    gPath = line.substring(p1 + 1, p2);
    if (gMethod != "GET" && gMethod != "POST") return false;
    if (gPath.length() > 160 || !gPath.startsWith("/")) return false;
    return true;
}

bool parseHeader(const String& line) {
    const int colon = line.indexOf(':');
    if (colon <= 0) return true;
    String name = line.substring(0, colon);
    String value = line.substring(colon + 1);
    name.trim(); name.toLowerCase(); value.trim();
    if (name == "content-length") {
        const long parsed = value.toInt();
        if (parsed < 0 || static_cast<size_t>(parsed) > MAX_BODY) return false;
        gContentLength = static_cast<size_t>(parsed);
    } else if (name == "x-maz-token") {
        if (value.length() > 160) return false;
        gToken = value;
    }
    return true;
}

void failRequest(int code, const char* text) {
    queueText(code, "text/plain", text);
    gRxState = RxState::Ready;
}

void consumeLine() {
    String line = gLine;
    gLine = "";
    if (line.endsWith("\r")) line.remove(line.length() - 1);

    if (gRxState == RxState::RequestLine) {
        if (!parseRequestLine(line)) {
            failRequest(400, "Invalid request line.");
            return;
        }
        gRxState = RxState::Headers;
        return;
    }

    if (gRxState != RxState::Headers) return;
    if (line.isEmpty()) {
        if (gContentLength > 0) {
            gBody.reserve(gContentLength);
            gRxState = RxState::Body;
        } else {
            dispatchRequest();
        }
        return;
    }
    gHeaderBytes += line.length() + 2;
    if (gHeaderBytes > MAX_HEADERS || !parseHeader(line)) {
        failRequest(400, "Headers too large or invalid.");
    }
}

void pumpRx() {
    size_t consumed = 0;
    while (gClient.connected() && gClient.available() &&
           consumed < RX_BUDGET_PER_TICK && gTxKind == TxKind::None) {
        const char c = static_cast<char>(gClient.read());
        ++consumed;
        gLastRxAt = millis();

        if (gRxState == RxState::Body) {
            if (gBody.length() < gContentLength) gBody += c;
            if (gBody.length() == gContentLength) dispatchRequest();
            continue;
        }
        if (gRxState == RxState::Ready) continue;

        if (c == '\n') {
            consumeLine();
        } else if (gLine.length() < MAX_LINE) {
            gLine += c;
        } else {
            failRequest(400, "Request line/header too long.");
        }
    }
}

size_t writeChunk(const uint8_t* data, size_t length) {
    if (!length || !gClient.connected()) return 0;
    const size_t amount = length > TX_BUDGET_PER_TICK ? TX_BUDGET_PER_TICK : length;
    const size_t wrote = gClient.write(data, amount);
    if (wrote) gLastTxAt = millis();
    return wrote;
}

void pumpTx() {
    if (gTxKind == TxKind::None || !gClient.connected()) return;

    if (gTxHeadAt < gTxHead.length()) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(gTxHead.c_str()) + gTxHeadAt;
        gTxHeadAt += writeChunk(ptr, gTxHead.length() - gTxHeadAt);
        return;
    }

    if (gTxKind == TxKind::Text) {
        if (gTxPayloadAt < gTxText.length()) {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(gTxText.c_str()) + gTxPayloadAt;
            gTxPayloadAt += writeChunk(ptr, gTxText.length() - gTxPayloadAt);
            return;
        }
        closeClient();
        return;
    }

    if (gTxKind == TxKind::Page) {
        const size_t total = strlen(PAGE);
        if (gTxPayloadAt < total) {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(PAGE) + gTxPayloadAt;
            gTxPayloadAt += writeChunk(ptr, total - gTxPayloadAt);
            return;
        }
        closeClient();
        return;
    }

    if (gTxKind == TxKind::Screen) {
        if (gTxPayloadAt >= SCREEN_BYTES) {
            closeClient();
            return;
        }
        const uint16_t* pixels = reinterpret_cast<const uint16_t*>(shell::canvas().getBuffer());
        if (!pixels) {
            closeClient();
            return;
        }
        const size_t pixelsAt = gTxPayloadAt / 2;
        const size_t remainingPixels = SCREEN_PIXELS - pixelsAt;
        const size_t maxPixels = sizeof(gScreenScratch) / 2;
        const size_t count = remainingPixels < maxPixels ? remainingPixels : maxPixels;
        for (size_t i = 0; i < count; ++i) {
            const uint16_t v = pixels[pixelsAt + i];
            gScreenScratch[i * 2] = static_cast<uint8_t>(v & 0xff);
            gScreenScratch[i * 2 + 1] = static_cast<uint8_t>(v >> 8);
        }
        gTxPayloadAt += writeChunk(gScreenScratch, count * 2);
    }
}

void acceptClient() {
    if (gClient && gClient.connected()) return;
    WiFiClient next = gServer.available();
    if (!next) return;
    gClient = next;
    gClient.setNoDelay(true);
    resetRx();
    resetTx();
    gOpenedAt = millis();
    gLastRxAt = gOpenedAt;
    gLastTxAt = gOpenedAt;
}

void enforceDeadlines() {
    if (!gClient || !gClient.connected()) return;
    const uint32_t now = millis();
    if (gTxKind == TxKind::None) {
        if (now - gLastRxAt > REQUEST_IDLE_MS || now - gOpenedAt > REQUEST_MAX_AGE_MS)
            failRequest(408, "Request timed out.");
    } else if (now - gLastTxAt > TX_IDLE_MS) {
        closeClient();
    }
}

void startServer() {
    if (gRunning || WiFi.status() != WL_CONNECTED) return;
    gServer.begin();
    gServer.setNoDelay(true);
    gRunning = true;
    if (!gMdns && MDNS.begin("mazpocket")) {
        MDNS.addService("http", "tcp", PORT);
        gMdns = true;
    }
}

}  // namespace

void begin() { startServer(); }

void update() {
    if (gReconnectAt && static_cast<int32_t>(millis() - gReconnectAt) >= 0) {
        gReconnectAt = 0;
        net::connectSaved();
    }
    if (gRebootAt && static_cast<int32_t>(millis() - gRebootAt) >= 0) {
        gRebootAt = 0;
        if (gToLauncher) launcher::reboot();
        else ESP.restart();
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (gClient) closeClient();
        gRunning = false;
        if (gMdns) {
            MDNS.end();
            gMdns = false;
        }
        return;
    }

    startServer();
    acceptClient();
    if (!gClient || !gClient.connected()) return;

    enforceDeadlines();
    if (gTxKind == TxKind::None) pumpRx();
    pumpTx();
}

}  // namespace portal
}  // namespace maz
