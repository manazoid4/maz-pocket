#include "portal.h"

#include <ESPmDNS.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "../audio/sfx.h"
#include "../core/launcher.h"
#include "../core/metrics.h"
#include "../core/settings.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../storage/store.h"
#include "action_ids.generated.h"
#include "host_async.h"
#include "mazhost.h"
#include "net.h"

namespace maz {
namespace portal {
namespace {

constexpr uint16_t PORT = 80;
constexpr size_t MAX_BODY = 1800;
WiFiServer gServer(PORT);
bool gRunning = false;
bool gMdns = false;
uint32_t gReconnectAt = 0;
uint32_t gRebootAt = 0;
bool gToLauncher = false;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0d10"><title>MAZ Pocket</title><style>
:root{color-scheme:dark;--bg:#0b0d10;--panel:#14181d;--panel2:#0f1317;--line:#29313a;--text:#f2f4f5;--dim:#9aa5af;--accent:#ff7a18;--ok:#47d78b;--warn:#ffbf47;--bad:#ff6469}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px ui-monospace,SFMono-Regular,Consolas,monospace}main{max-width:980px;margin:auto;padding:14px}.top{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:8px 0 13px;border-bottom:1px solid var(--line);position:sticky;top:0;background:rgba(11,13,16,.95);backdrop-filter:blur(8px);z-index:4}.brand{font-size:20px;font-weight:800}.brand b{color:var(--accent)}.pills{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}.pill{border:1px solid var(--line);border-radius:999px;padding:5px 8px;color:var(--dim);font-size:11px}.pill.ok{color:var(--ok);border-color:#25583f}.pill.bad{color:var(--bad);border-color:#633336}.hero{padding:18px 0 10px}.hero h1{font-size:25px;margin:0 0 6px}.hero p{margin:0;color:var(--dim);line-height:1.5}.section{margin-top:14px}.section h2{font-size:11px;letter-spacing:.14em;color:var(--dim);margin:0 0 8px}.surfaces,.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}.surface,.card{border:1px solid var(--line);background:var(--panel);border-radius:10px;padding:12px}.surface{min-height:104px;display:flex;flex-direction:column;justify-content:space-between}.surface strong{font-size:16px}.surface p{color:var(--dim);margin:5px 0 10px;font-size:12px;line-height:1.35}.surface button{width:100%}.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.kv{display:grid;grid-template-columns:1fr auto;gap:7px 10px}.kv span{color:var(--dim)}button,input,select{font:inherit;border:1px solid var(--line);border-radius:7px;background:#090c0f;color:var(--text);padding:9px}button{cursor:pointer}button.primary{background:var(--accent);border-color:var(--accent);color:#08090a;font-weight:800}button:active{transform:translateY(1px)}.row{display:flex;gap:6px;flex-wrap:wrap}.row button{flex:1;min-width:104px}.config{display:grid;grid-template-columns:1fr 1fr;gap:7px}.config input,.config select{width:100%}.wide{grid-column:1/-1}.note{color:var(--dim);font-size:12px;line-height:1.45}.status{min-height:34px;margin-top:8px;padding:8px;border-radius:7px;background:var(--panel2);color:var(--dim);white-space:pre-wrap}.accent{color:var(--accent)}.oktxt{color:var(--ok)}.warntxt{color:var(--warn)}.badtxt{color:var(--bad)}.hide{display:none}@media(max-width:700px){main{padding:10px}.surfaces{grid-template-columns:repeat(2,minmax(0,1fr))}.grid{grid-template-columns:1fr}.top{align-items:flex-start}.hero h1{font-size:21px}}@media(max-width:390px){.surfaces{grid-template-columns:1fr 1fr}.surface{padding:10px;min-height:110px}.surface strong{font-size:14px}.pills{max-width:58%}}
</style></head><body><main>
<div class="top"><div class="brand"><b>MAZ</b> POCKET <span id="ver" class="note">v-</span></div><div class="pills"><span id="pwifi" class="pill">Wi-Fi</span><span id="phost" class="pill">PC</span><span id="pagents" class="pill">Agents</span><span id="pbat" class="pill">Battery</span></div></div>
<div class="hero"><h1>Your pocket control surface</h1><p>Open a mode on the Cardputer, control the PC, check agents, configure MAZ Core and inspect live runtime health from one page.</p></div>

<div class="section"><h2>PRIMARY SURFACES</h2><div class="surfaces">
<div class="surface"><div><strong>COMM</strong><p>Stream voice to MAZ and control the PC.</p></div><button onclick="openApp('talk')">OPEN ON DEVICE</button></div>
<div class="surface"><div><strong>CAPTURE</strong><p>Voice notes and lossless BrainDump capture.</p></div><button onclick="openApp('braindump')">OPEN ON DEVICE</button></div>
<div class="surface"><div><strong>OPS</strong><p>Agent Nudge state and actions.</p></div><button onclick="openApp('nudge')">OPEN ON DEVICE</button></div>
<div class="surface"><div><strong>CONTROL</strong><p>Connections, PC, Core and device tools.</p></div><button onclick="openApp('desk')">OPEN ON DEVICE</button></div>
<div class="surface"><div><strong>RECALL</strong><p>Inbox, notes, snippets and viewer.</p></div><button onclick="openApp('recall')">OPEN ON DEVICE</button></div>
<div class="surface"><div><strong>FLOW</strong><p>Reminders, focus, sprint and tasks.</p></div><button onclick="openApp('flow')">OPEN ON DEVICE</button></div>
</div></div>

<div class="section"><h2>LIVE STATUS</h2><div class="grid">
<div class="card"><div class="kv"><span>Current app</span><b id="app">-</b><span>Wi-Fi</span><b id="wifi">-</b><span>IP</span><b id="ip">-</b><span>Signal</span><b id="rssi">-</b><span>Storage</span><b id="storage">-</b><span>SD</span><b id="sd">-</b></div><div class="row" style="margin-top:10px"><button onclick="act('reconnect')">RECONNECT WI-FI</button><button onclick="copyIp()">COPY IP</button></div></div>
<div class="card"><div class="kv"><span>MAZ Host</span><b id="host">-</b><span>Route</span><b id="routeText">-</b><span>Local brain</span><b class="accent">LFM2.5 8B A1B</b><span>Working</span><b id="working">0</b><span>Waiting</span><b id="waiting">0</b><span>Stale</span><b id="stale">0</b></div><div class="row" style="margin-top:10px"><button onclick="act('host')">HOST STATUS</button><button onclick="openApp('nudge')">OPEN OPS</button></div></div>
<div class="card"><div class="kv"><span>Free heap</span><b id="heap">-</b><span>Heap floor</span><b id="heapMin">-</b><span>Largest block</span><b id="heapBlock">-</b><span>Main stack</span><b id="mainStack">-</b><span>Host queue</span><b id="hostQ">-</b><span>Host max latency</span><b id="hostLat">-</b></div></div>
<div class="card"><div class="kv"><span>WS queue</span><b id="wsQ">-</b><span>Frames sent</span><b id="wsSent">-</b><span>Frame drops</span><b id="wsDrop">-</b><span>Reconnects</span><b id="wsReconnect">-</b><span>First token</span><b id="wsFirst">-</b><span>Turn total</span><b id="wsTotal">-</b></div></div>
</div></div>

<div class="section"><h2>PC QUICK CONTROL</h2><div class="card"><div class="row"><button onclick="pc('desktop')">DESKTOP</button><button onclick="pc('play_pause')">PLAY / PAUSE</button><button onclick="pc('mute')">MUTE</button><button onclick="pc('volume_down')">VOL -</button><button onclick="pc('volume_up')">VOL +</button><button onclick="pc('previous_track')">PREV</button><button onclick="pc('next_track')">NEXT</button><button onclick="pc('lock')">LOCK</button></div><p class="note">PC commands are queued to the Host worker immediately; this web request never blocks the Cardputer UI on the laptop.</p></div></div>

<div class="section"><h2>UNLOCK + CONFIGURE</h2><div class="grid">
<div class="card"><div class="note">Read-only status works immediately. Enter the same MAZ pairing token used by MAZ Host to enable controls.</div><div class="config" style="margin-top:9px"><input class="wide" id="token" type="password" placeholder="MAZ pairing token"><button class="primary wide" onclick="unlock()">UNLOCK CONTROLS</button></div><div id="lock" class="status">Controls locked.</div></div>
<div class="card"><div id="cfg" class="config"><input id="hostAddr" placeholder="MAZ Host IP/name"><input id="hostPort" type="number" value="8787" placeholder="Port"><input class="wide" id="remote" placeholder="Optional remote https://..."><select id="route"><option value="0">LOCAL</option><option value="1">AUTO</option><option value="2">CLOUD</option></select><select id="tts"><option value="1">Spoken replies ON</option><option value="0">Spoken replies OFF</option></select><input id="ssid2" placeholder="Backup Wi-Fi SSID"><input id="pass2" type="password" placeholder="Backup Wi-Fi password"><button class="primary wide" onclick="saveCfg()">SAVE CONFIG</button></div></div>
</div></div>

<div class="section"><h2>DEVICE</h2><div class="card"><div class="row"><button onclick="act('speaker')">SPEAKER TEST</button><button onclick="act('storage')">SD CHECK</button><button onclick="act('reboot')">REBOOT MAZ</button><button onclick="act('launcher')">RETURN TO M5LAUNCHER</button></div><p class="note">Updates belong to M5Launcher. MAZ Pocket contains no generic OTA writer and does not own other Launcher partitions.</p><div id="log" class="status">Ready.</div></div></div>
</main><script>
const $=id=>document.getElementById(id);let token=sessionStorage.getItem('mazToken')||'';if(token)$('token').value=token;const headers=()=>token?{'X-MAZ-Token':token}:{};function log(x,bad=false){$('log').textContent=x;$('log').className='status '+(bad?'badtxt':'')};function pill(id,good,text){let e=$(id);e.textContent=text;e.className='pill '+(good?'ok':'bad')};const kb=n=>(n/1024).toFixed(0)+' KB';async function status(fill=false){try{let r=await fetch('/api/status',{headers:headers(),cache:'no-store'});let d=await r.json();$('ver').textContent='v'+(d.version||'-');$('app').textContent=d.app||'-';$('wifi').textContent=d.wifi_ssid||'offline';$('ip').textContent=d.ip||'-';$('rssi').textContent=d.rssi?d.rssi+' dBm':'-';$('storage').textContent=d.storage||'-';$('sd').textContent=d.sd_present?'ready':(d.sd_unreadable?'unreadable':'none');$('host').textContent=d.host_link||'OFFLINE';$('routeText').textContent=['LOCAL','AUTO','CLOUD'][d.route||0];$('working').textContent=d.agents_working||0;$('waiting').textContent=d.agents_waiting||0;$('stale').textContent=d.agents_stale||0;$('heap').textContent=kb(d.free_heap||0);$('heapMin').textContent=kb(d.min_free_heap||0);$('heapBlock').textContent=kb(d.largest_free_block||0);$('mainStack').textContent=(d.main_stack_words||0)+' w';$('hostQ').textContent=(d.host_queue_depth||0)+' / '+(d.host_queue_high_water||0);$('hostLat').textContent=(d.host_max_latency_ms||0)+' ms';$('wsQ').textContent=(d.ws_queue_depth||0)+' / '+(d.ws_queue_high_water||0);$('wsSent').textContent=d.ws_frames_sent||0;$('wsDrop').textContent=d.ws_frame_drops||0;$('wsReconnect').textContent=d.ws_reconnects||0;$('wsFirst').textContent=(d.ws_first_token_ms||0)+' ms';$('wsTotal').textContent=(d.ws_total_ms||0)+' ms';pill('pwifi',!!d.wifi_connected,d.wifi_connected?'Wi-Fi ✓':'Wi-Fi ×');pill('phost',d.host_link&&d.host_link!='OFFLINE',d.host_link&&d.host_link!='OFFLINE'?'PC ✓':'PC ×');pill('pagents',!(d.agents_stale||d.agent_question),(d.agent_question?'Needs Maz':(d.agents_stale?d.agents_stale+' stale':'Agents ✓')));pill('pbat',(d.battery||0)>15,(d.battery<0?'Battery ?':'Battery '+d.battery+'%'));$('lock').innerHTML=d.unlocked?'<span class="oktxt">CONTROLS UNLOCKED</span>':'Controls locked.';if(fill&&d.unlocked){$('hostAddr').value=d.host_addr||'';$('hostPort').value=d.host_port||8787;$('remote').value=d.remote_url||'';$('route').value=String(d.route||0);$('tts').value=d.tts?'1':'0';$('ssid2').value=d.ssid2||'';}}catch(e){pill('pwifi',false,'Device offline');log('Lost connection to MAZ Pocket.',true)}}async function unlock(){token=$('token').value.trim();sessionStorage.setItem('mazToken',token);await status(true)}async function post(path,body){let r=await fetch(path,{method:'POST',headers:{...headers(),'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)});let text=await r.text();if(!r.ok){log(text,true);return null}log(text);setTimeout(()=>status(false),250);return text}function act(a){return post('/api/action',{action:a})}function openApp(a){return act('open:'+a)}function pc(a){return act('pc:'+a)}async function saveCfg(){let out=await post('/api/config',{host_addr:$('hostAddr').value.trim(),host_port:$('hostPort').value||'8787',remote_url:$('remote').value.trim(),route:$('route').value,tts:$('tts').value,ssid2:$('ssid2').value.trim(),pass2:$('pass2').value});if(out)setTimeout(()=>status(true),500)}function copyIp(){navigator.clipboard?.writeText($('ip').textContent);log('IP copied: '+$('ip').textContent)}status(true);setInterval(()=>status(false),2500);
</script></body></html>)HTML";

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

void sendHead(WiFiClient& c, int code, const char* type, size_t len) {
    c.print("HTTP/1.1 ");
    c.print(code);
    c.print(code == 200 ? " OK\r\n" : code == 401 ? " Unauthorized\r\n" : " Error\r\n");
    c.print("Content-Type: ");
    c.print(type);
    c.print("\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ");
    c.print(len);
    c.print("\r\n\r\n");
}

void sendText(WiFiClient& c, int code, const char* type, const String& body) {
    sendHead(c, code, type, body.length());
    c.print(body);
}

String statusJson(bool unlocked) {
    const auto m = metrics::snapshot();
    String j;
    j.reserve(1700);
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
    j += ",\"agents_working\":" + String(Sys.agentsWorking);
    j += ",\"agents_waiting\":" + String(Sys.agentsWaiting);
    j += ",\"agents_stale\":" + String(Sys.agentsStale);
    j += ",\"agent_question\":"; j += Sys.agentQuestion ? "true" : "false";
    j += ",\"route\":" + String(Cfg.talkRoute);
    j += ",\"tts\":"; j += Cfg.ttsEnabled ? "true" : "false";
    j += ",\"uptime_ms\":" + String(m.uptimeMs);
    j += ",\"free_heap\":" + String(m.freeHeap);
    j += ",\"min_free_heap\":" + String(m.minFreeHeap);
    j += ",\"largest_free_block\":" + String(m.largestFreeBlock);
    j += ",\"main_stack_words\":" + String(m.mainStackMinWords);
    j += ",\"host_queue_depth\":" + String(m.hostQueueDepth);
    j += ",\"host_queue_high_water\":" + String(m.hostQueueHighWater);
    j += ",\"host_rejected\":" + String(m.hostRejected);
    j += ",\"host_result_drops\":" + String(m.hostResultDrops);
    j += ",\"host_last_latency_ms\":" + String(m.hostLastLatencyMs);
    j += ",\"host_max_latency_ms\":" + String(m.hostMaxLatencyMs);
    j += ",\"host_stack_words\":" + String(m.hostStackMinWords);
    j += ",\"ws_queue_depth\":" + String(m.wsQueueDepth);
    j += ",\"ws_queue_high_water\":" + String(m.wsQueueHighWater);
    j += ",\"ws_frames_sent\":" + String(m.wsFramesSent);
    j += ",\"ws_frame_drops\":" + String(m.wsFrameDrops);
    j += ",\"ws_reconnects\":" + String(m.wsReconnects);
    j += ",\"ws_protocol_errors\":" + String(m.wsProtocolErrors);
    j += ",\"ws_first_token_ms\":" + String(m.wsFirstTokenMs);
    j += ",\"ws_total_ms\":" + String(m.wsTotalMs);
    j += ",\"ws_stack_words\":" + String(m.wsStackMinWords);
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
           id == "network" || id == "core" || id == "tools" || id == "settings";
}

bool allowedPc(const String& id) {
    for (size_t i = 0; i < action_ids::PC_COUNT; ++i)
        if (id == action_ids::PC[i]) return true;
    return false;
}

String runAction(const String& body) {
    const String action = formValue(body, "action");
    if (action == "reconnect") {
        gReconnectAt = millis() + 150;
        return "Wi-Fi reconnect requested.";
    }
    if (action == "host") {
        return Sys.hostOnline ? "MAZ Host is currently online." : "MAZ Host is currently offline.";
    }
    if (action == "speaker") {
        sfx::lineOpen();
        return "Speaker test played.";
    }
    if (action == "storage") {
        if (Sys.storage != Storage::SD) return "SD is not the active storage backend.";
        const std::string path = "/maz/cache/.portal-check";
        const std::string probe = "MAZ-PORTAL-CHECK";
        if (!store::writeText(path, probe)) return "SD write failed.";
        const bool ok = store::readText(path, 64) == probe;
        store::remove(path);
        return ok ? "SD read/write check passed." : "SD verification failed.";
    }
    if (action == "reboot") {
        gToLauncher = false;
        gRebootAt = millis() + 500;
        return "Rebooting MAZ Pocket...";
    }
    if (action == "launcher") {
        gToLauncher = true;
        gRebootAt = millis() + 500;
        return "Returning to M5Launcher...";
    }
    if (action.startsWith("open:")) {
        const String id = action.substring(5);
        if (!allowedApp(id)) return "Unsupported app.";
        shell::goHome();
        if (!shell::pushById(id.c_str())) return "Could not open app.";
        shell::wake();
        return String("Opened ") + id + " on the Cardputer.";
    }
    if (action.startsWith("pc:")) {
        const String id = action.substring(3);
        if (!allowedPc(id)) return "Unsupported PC action.";
        const uint32_t request = host_async::pcAction(id.c_str());
        return request ? String("PC action queued: ") + id : "Host queue busy; try again.";
    }
    return "Unknown action.";
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

void serve(WiFiClient& c) {
    c.setTimeout(800);
    String line = c.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) return;

    const int p1 = line.indexOf(' ');
    const int p2 = line.indexOf(' ', p1 + 1);
    if (p1 < 0 || p2 < 0) return;
    const String method = line.substring(0, p1);
    const String path = line.substring(p1 + 1, p2);

    size_t contentLength = 0;
    String token;
    while (c.connected()) {
        String h = c.readStringUntil('\n');
        h.trim();
        if (h.isEmpty()) break;
        if (h.startsWith("Content-Length:")) {
            contentLength = static_cast<size_t>(h.substring(15).toInt());
        } else if (h.startsWith("X-MAZ-Token:")) {
            token = h.substring(12);
            token.trim();
        }
    }

    if (contentLength > MAX_BODY) {
        sendText(c, 400, "text/plain", "Request too large.");
        return;
    }
    String body;
    while (body.length() < contentLength && c.connected()) {
        while (c.available() && body.length() < contentLength) body += static_cast<char>(c.read());
        delay(1);
    }

    const bool unlocked = tokenOk(token);
    if (method == "GET" && (path == "/" || path == "/index.html")) {
        sendHead(c, 200, "text/html; charset=utf-8", strlen(PAGE));
        c.print(PAGE);
    } else if (method == "GET" && path == "/api/status") {
        sendText(c, 200, "application/json", statusJson(unlocked));
    } else if (method == "POST" && path == "/api/action") {
        if (!unlocked) sendText(c, 401, "text/plain", "Unlock controls with the MAZ pairing token first.");
        else sendText(c, 200, "text/plain", runAction(body));
    } else if (method == "POST" && path == "/api/config") {
        if (!unlocked) sendText(c, 401, "text/plain", "Unlock controls with the MAZ pairing token first.");
        else sendText(c, 200, "text/plain", saveConfig(body));
    } else {
        sendText(c, 404, "text/plain", "Not found.");
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
        gRunning = false;
        return;
    }
    startServer();
    WiFiClient c = gServer.available();
    if (!c) return;
    serve(c);
    c.stop();
}

}  // namespace portal
}  // namespace maz
