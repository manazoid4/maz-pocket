// MAZ Pocket v0.5 local control plane.
// One page mirrors the device's actual capabilities: live screen, surfaces,
// Wi-Fi provisioning, PC actions, MAZ Core config, diagnostics and recovery.
#include "web.h"

#include <ESPmDNS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_system.h>

#include "../audio/sfx.h"
#include "../core/launcher.h"
#include "../core/settings.h"
#include "../core/shell.h"
#include "../core/power.h"
#include "../core/sys.h"
#include "../storage/store.h"
#include "mazhost.h"
#include "net.h"

namespace maz {
namespace web {
namespace {

constexpr uint16_t PORT = 80;
constexpr size_t MAX_BODY = 2400;
constexpr size_t SCREEN_BYTES = 240u * 135u * 2u;

WiFiServer gServer(PORT);
bool gRunning = false;
bool gMdns = false;

enum class Pending : uint8_t { None, Reboot, Launcher };
Pending gPending = Pending::None;
uint32_t gPendingAt = 0;

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><meta name=robots content="noindex,nofollow"><title>MAZ Pocket v0.5</title><style>
:root{color-scheme:dark;--bg:#080a0c;--panel:#11161b;--line:#29323b;--text:#eef2f5;--dim:#8996a3;--orange:#ff7a18;--green:#45df88;--red:#ff5d68;--cyan:#45ccef}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px ui-monospace,Consolas,monospace}main{max-width:980px;margin:auto;padding:14px}header{display:flex;justify-content:space-between;gap:12px;align-items:end;border-bottom:1px solid var(--line);padding:4px 0 12px;margin-bottom:12px}h1{font-size:22px;margin:0}.orange{color:var(--orange)}.dim{color:var(--dim)}.good{color:var(--green)}.bad{color:var(--red)}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:10px}.card{background:var(--panel);border:1px solid var(--line);border-radius:9px;padding:12px}.card h2{font-size:11px;letter-spacing:.14em;color:var(--orange);margin:0 0 10px}.kv{display:grid;grid-template-columns:1fr auto;gap:6px}.buttons{display:flex;flex-wrap:wrap;gap:6px}button,input,select{font:inherit;border:1px solid var(--line);border-radius:6px;padding:9px;background:#090c0f;color:var(--text)}button{cursor:pointer}button.primary{background:var(--orange);color:#08090a;border-color:var(--orange);font-weight:700}input,select{width:100%;margin:3px 0 8px}label{display:block;color:var(--dim);font-size:12px}canvas{width:100%;max-width:480px;image-rendering:pixelated;border:1px solid var(--line);border-radius:6px;background:#000}pre{white-space:pre-wrap;word-break:break-word;min-height:50px;color:var(--dim);margin:0}.wide{grid-column:1/-1}.pill{display:inline-block;border:1px solid var(--line);border-radius:999px;padding:4px 7px;margin:2px 3px 2px 0;font-size:12px}@media(max-width:520px){header{display:block}main{padding:10px}.grid{grid-template-columns:1fr}}</style></head><body><main>
<header><div><h1><span class=orange>MAZ</span> POCKET <span class=dim>v0.5</span></h1><div class=dim>Everything useful, one control surface</div></div><div id=quick class=dim>loading...</div></header>
<div class=grid>
<section class=card><h2>UNLOCK</h2><label>MAZ pairing token</label><input id=token type=password placeholder="token stays in this browser session"><button class=primary onclick=unlock()>UNLOCK</button><div id=auth class=dim>Read-only until unlocked. Wi-Fi setup remains available on the setup hotspot.</div></section>
<section class=card><h2>DEVICE</h2><div class=kv><span>App</span><b id=app>-</b><span>Battery</span><b id=battery>-</b><span>Storage</span><b id=storage>-</b><span>Free heap</span><b id=heap>-</b><span>SD</span><b id=sd>-</b></div></section>
<section class=card><h2>NETWORK</h2><div class=kv><span>Wi-Fi</span><b id=wifi>-</b><span>IP</span><b id=ip>-</b><span>Signal</span><b id=rssi>-</b><span>Setup AP</span><b id=ap>-</b><span>MAZ Core</span><b id=host>-</b></div><div class=buttons style="margin-top:9px"><button onclick="action('reconnect')">RECONNECT</button><button onclick="action('disconnect')">DISCONNECT</button><button onclick="action('setup_ap')">SETUP HOTSPOT</button></div></section>
<section class=card><h2>PRIMARY WI-FI</h2><input id=ssid1 placeholder="SSID"><input id=pass1 type=password placeholder="password"><button class=primary onclick="wifiSave(1)">CONNECT + SAVE</button></section>
<section class=card><h2>BACKUP WI-FI</h2><input id=ssid2 placeholder="SSID"><input id=pass2 type=password placeholder="password"><button onclick="wifiSave(2)">SAVE BACKUP</button></section>
<section class=card><h2>MAZ CORE</h2><input id=hostAddr placeholder="PC IP / hostname"><input id=hostPort type=number value=8787><input id=remote placeholder="optional HTTPS/Tailscale URL"><select id=route><option value=mazlatest>MAZLATEST</option><option value=cloud>CLOUD</option><option value=local>LOCAL</option><option value=auto>AUTO</option></select><label>Host token (only enter to replace)</label><input id=hostToken type=password placeholder="leave blank to keep"><button onclick=saveCore()>SAVE CORE</button><button onclick="action('host_test')">TEST CORE</button></section>
<section class=card><h2>SURFACES</h2><div class=buttons><button class=primary onclick="surface('talk')">COMM</button><button onclick="surface('braindump')">CAPTURE</button><button onclick="surface('nudge')">OPS</button><button onclick="surface('desk')">CONTROL</button><button onclick="surface('recall')">RECALL</button><button onclick="surface('flow')">FLOW</button><button onclick="surface('network')">WI-FI</button><button onclick="surface('control')">CONTROL CENTER</button></div></section>
<section class=card><h2>PC QUICK CONTROL</h2><div class=buttons><button onclick="pc('desktop')">DESKTOP</button><button onclick="pc('play_pause')">PLAY/PAUSE</button><button onclick="pc('mute')">MUTE</button><button onclick="pc('volume_down')">VOL-</button><button onclick="pc('volume_up')">VOL+</button><button onclick="pc('previous_track')">PREV</button><button onclick="pc('next_track')">NEXT</button><button onclick="pc('lock')">LOCK</button></div></section>
<section class="card wide"><h2>LIVE CARDPUTER SCREEN</h2><canvas id=screen width=240 height=135></canvas><div class=dim>2 FPS RGB565 mirror. This is the Cardputer LCD stream; a camera feed requires external camera hardware.</div></section>
<section class=card><h2>DIAGNOSTICS</h2><div class=buttons><button onclick="action('speaker')">SPEAKER</button><button onclick="action('host_test')">CORE</button><button onclick="action('storage')">STORAGE</button><button onclick="surface('tools')">ALL TESTS</button></div></section>
<section class=card><h2>RECOVERY / FIRMWARE</h2><div class=buttons><button onclick="action('reboot')">REBOOT MAZ</button><button class=primary onclick="action('launcher')">M5LAUNCHER</button></div><p class=dim>Firmware installs stay in M5Launcher so MAZ cannot overwrite another app partition.</p></section>
<section class="card wide"><h2>EVENT LOG</h2><pre id=log>Ready.</pre></section>
</div></main><script>
const $=id=>document.getElementById(id);let T=sessionStorage.getItem('mazToken')||'';if(T)$('token').value=T;const headers=()=>T?{'X-MAZ-Token':T}:{};const log=x=>$('log').textContent=(typeof x==='string'?x:JSON.stringify(x,null,2));async function getStatus(fill=false){try{const r=await fetch('/api/status',{cache:'no-store',headers:headers()});const d=await r.json();$('app').textContent=d.app||'-';$('battery').textContent=(d.battery<0?'?':d.battery+'%')+(d.power_trend==='rising'?' \u2191':d.power_trend==='falling'?' \u2193':'');$('storage').textContent=d.storage||'-';$('heap').textContent=Math.round((d.heap||0)/1024)+' KB';$('sd').innerHTML=d.sd_unreadable?'<span class=bad>UNREADABLE</span>':d.sd_present?'<span class=good>READY</span>':'none';$('wifi').textContent=d.wifi_ssid||'offline';$('ip').textContent=d.ip||'-';$('rssi').textContent=d.rssi?d.rssi+' dBm':'-';$('ap').textContent=d.setup_ap?'ON':'off';$('host').textContent=d.host_link||'OFFLINE';$('auth').innerHTML=d.unlocked?'<span class=good>CONTROLS UNLOCKED</span>':'Read-only until unlocked.';$('quick').textContent=(d.wifi_connected?'WiFi ✓':'WiFi –')+' · PC '+(d.host_online?'✓':'–')+' · '+(d.agents_working||0)+' agents';if(fill&&d.unlocked){$('ssid1').value=d.wifi_primary||'';$('ssid2').value=d.wifi_backup||'';$('hostAddr').value=d.host_addr||'';$('hostPort').value=d.host_port||8787;$('remote').value=d.remote_url||'';$('route').value=d.route_id||['local','auto','cloud','mazlatest'][d.route??2]}}catch(e){$('quick').textContent='device unreachable'}}async function unlock(){T=$('token').value.trim();sessionStorage.setItem('mazToken',T);await getStatus(true)}async function post(path,body){const r=await fetch(path,{method:'POST',headers:{...headers(),'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)});const text=await r.text();try{log(JSON.parse(text))}catch{log(text)}setTimeout(()=>getStatus(true),250);return r.ok}function action(a){return post('/api/action',{action:a})}function surface(id){return post('/api/action',{action:'surface',target:id})}function pc(a){return post('/api/action',{action:'pc',target:a})}function wifiSave(slot){return post('/api/wifi',{slot:String(slot),ssid:$(slot===1?'ssid1':'ssid2').value.trim(),pass:$(slot===1?'pass1':'pass2').value})}function saveCore(){return post('/api/config',{host_addr:$('hostAddr').value.trim(),host_port:$('hostPort').value||'8787',remote_url:$('remote').value.trim(),route:$('route').value,host_token:$('hostToken').value.trim()})}
const ctx=$('screen').getContext('2d'),img=ctx.createImageData(240,135);async function frame(){if(!T)return;try{const r=await fetch('/api/screen',{cache:'no-store',headers:headers()});if(!r.ok)return;const b=new Uint8Array(await r.arrayBuffer());if(b.length!==64800)return;for(let p=0,i=0;p<32400;p++,i+=2){const v=b[i]|(b[i+1]<<8),o=p*4;img.data[o]=((v>>11)&31)*255/31;img.data[o+1]=((v>>5)&63)*255/63;img.data[o+2]=(v&31)*255/31;img.data[o+3]=255}ctx.putImageData(img,0,0)}catch{}}getStatus(true);setInterval(()=>getStatus(false),2500);setInterval(frame,500);
</script></body></html>)HTML";

String jsonEscape(const String& in) {
    String out; out.reserve(in.length() + 8);
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
    String out; out.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%' && i + 2 < value.length()) {
            const int a = hexNibble(value[i + 1]), b = hexNibble(value[i + 2]);
            if (a >= 0 && b >= 0) { out += static_cast<char>((a << 4) | b); i += 2; continue; }
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
    c.print("HTTP/1.1 "); c.print(code);
    c.print(code == 200 ? " OK\r\n" : code == 401 ? " Unauthorized\r\n" : code == 404 ? " Not Found\r\n" : " Error\r\n");
    c.print("Content-Type: "); c.print(type);
    c.print("\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: ");
    c.print(len); c.print("\r\n\r\n");
}

void sendText(WiFiClient& c, int code, const char* type, const String& body) {
    sendHead(c, code, type, body.length()); c.print(body);
}

void sendPage(WiFiClient& c) {
    const size_t n = strlen(PAGE);
    sendHead(c, 200, "text/html; charset=utf-8", n);
    c.print(PAGE);
}

String statusJson(bool unlocked) {
    String j; j.reserve(1100);
    j += "{\"ok\":true,\"version\":\"" MAZ_POCKET_VERSION "\"";
    j += ",\"unlocked\":"; j += unlocked ? "true" : "false";
    j += ",\"app\":\"" + jsonEscape(shell::currentId()) + "\"";
    j += ",\"battery\":" + String(Sys.batteryPct);
    j += ",\"charging\":"; j += Sys.charging ? "true" : "false";  // true only when power_trend is rising
    j += ",\"power_trend\":\"" + String(power::trendName()) + "\",\"battery_mv\":" + String(Sys.batteryMv);
    j += ",\"storage\":\"" + String(store::backendName()) + "\"";
    j += ",\"free\":" + String(static_cast<unsigned long long>(store::freeBytes()));
    j += ",\"heap\":" + String(ESP.getFreeHeap());
    j += ",\"sd_present\":"; j += Sys.sdPresent ? "true" : "false";
    j += ",\"sd_unreadable\":"; j += Sys.sdUnreadable ? "true" : "false";
    j += ",\"wifi_connected\":"; j += Sys.wifiConnected ? "true" : "false";
    j += ",\"wifi_ssid\":\"" + jsonEscape(Sys.wifiSsid.c_str()) + "\"";
    j += ",\"ip\":\"" + jsonEscape(Sys.ip.c_str()) + "\"";
    j += ",\"rssi\":" + String(net::rssi());
    j += ",\"setup_ap\":"; j += net::setupApActive() ? "true" : "false";
    j += ",\"host_online\":"; j += Sys.hostOnline ? "true" : "false";
    j += ",\"host_link\":\"" + jsonEscape(host::linkName()) + "\"";
    j += ",\"agents_working\":" + String(Sys.agentsWorking);
    j += ",\"agents_waiting\":" + String(Sys.agentsWaiting);
    j += ",\"agents_stale\":" + String(Sys.agentsStale);
    if (unlocked) {
        j += ",\"wifi_primary\":\"" + jsonEscape(Cfg.wifiSsid.c_str()) + "\"";
        j += ",\"wifi_backup\":\"" + jsonEscape(Cfg.wifiSsid2.c_str()) + "\"";
        j += ",\"host_addr\":\"" + jsonEscape(Cfg.hostAddr.c_str()) + "\"";
        j += ",\"host_port\":" + String(Cfg.hostPort);
        j += ",\"remote_url\":\"" + jsonEscape(Cfg.hostRemoteUrl.c_str()) + "\"";
        j += ",\"route\":" + String(Cfg.talkRoute);
        j += ",\"route_id\":\"" + String(talkRouteApiName(Cfg.talkRoute)) + "\"";
    }
    j += '}'; return j;
}

String storageCheck() {
    if (!store::ready()) return "{\"ok\":false,\"error\":\"storage_unavailable\"}";
    const std::string path = "/maz/cache/.web-check";
    const std::string probe = "MAZ-V05-STORAGE";
    if (!store::writeText(path, probe)) return "{\"ok\":false,\"error\":\"write_failed\"}";
    const std::string got = store::readText(path, 64);
    store::remove(path);
    return got == probe ? "{\"ok\":true,\"result\":\"read_write_ok\"}" : "{\"ok\":false,\"error\":\"verify_failed\"}";
}

String performAction(const String& body) {
    const String name = formValue(body, "action");
    const String target = formValue(body, "target");
    if (name == "surface") {
        const bool ok = shell::pushById(target.c_str());
        shell::wake();
        return ok ? "{\"ok\":true,\"result\":\"surface_opened\"}" : "{\"ok\":false,\"error\":\"unknown_surface\"}";
    }
    if (name == "pc") {
        const host::Reply reply = host::pcAction(target.c_str());
        return reply.ok ? "{\"ok\":true,\"result\":\"" + jsonEscape(reply.text.c_str()) + "\"}" : "{\"ok\":false,\"error\":\"" + jsonEscape(reply.error.c_str()) + "\"}";
    }
    if (name == "reconnect") {
        net::disconnect();
        const bool ok = net::connectSaved();
        if (!ok) net::startSetupAp();
        return ok ? "{\"ok\":true,\"result\":\"connected\"}" : "{\"ok\":false,\"error\":\"saved_networks_failed_setup_ap_on\"}";
    }
    if (name == "disconnect") { net::disconnect(); return "{\"ok\":true,\"result\":\"disconnected\"}"; }
    if (name == "setup_ap") return net::startSetupAp() ? "{\"ok\":true,\"result\":\"MAZ-Pocket-Setup / 192.168.4.1\"}" : "{\"ok\":false,\"error\":\"setup_ap_failed\"}";
    if (name == "speaker") { sfx::boot(); return "{\"ok\":true,\"result\":\"speaker_test_played\"}"; }
    if (name == "host_test") return host::health() ? "{\"ok\":true,\"result\":\"maz_core_online\"}" : "{\"ok\":false,\"error\":\"maz_core_offline\"}";
    if (name == "storage") return storageCheck();
    if (name == "reboot") { gPending = Pending::Reboot; gPendingAt = millis() + 350; return "{\"ok\":true,\"result\":\"rebooting\"}"; }
    if (name == "launcher") { gPending = Pending::Launcher; gPendingAt = millis() + 350; return "{\"ok\":true,\"result\":\"opening_m5launcher\"}"; }
    return "{\"ok\":false,\"error\":\"unknown_action\"}";
}

String saveWifi(const String& body) {
    const int slot = formValue(body, "slot").toInt();
    const String ssid = formValue(body, "ssid"), pass = formValue(body, "pass");
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 96)
        return "{\"ok\":false,\"error\":\"invalid_wifi_fields\"}";
    if (slot == 2) {
        Cfg.wifiSsid2 = ssid.c_str(); Cfg.wifiPass2 = pass.c_str(); Cfg.save();
        return "{\"ok\":true,\"result\":\"backup_saved\"}";
    }
    const bool ok = net::connect(ssid.c_str(), pass.c_str());
    if (!ok) { net::startSetupAp(); return "{\"ok\":false,\"error\":\"connection_failed\"}"; }
    Cfg.wifiSsid = ssid.c_str(); Cfg.wifiPass = pass.c_str(); Cfg.save();
    return "{\"ok\":true,\"result\":\"primary_connected_saved\"}";
}

String saveConfig(const String& body) {
    const String addr = formValue(body, "host_addr"), port = formValue(body, "host_port"), remote = formValue(body, "remote_url"), route = formValue(body, "route"), token = formValue(body, "host_token");
    if (addr.length() > 120 || remote.length() > 240 || token.length() > 240) return "{\"ok\":false,\"error\":\"field_too_long\"}";
    Cfg.hostAddr = addr.c_str();
    const int p = port.toInt(); if (p > 0 && p < 65536) Cfg.hostPort = static_cast<uint16_t>(p);
    Cfg.hostRemoteUrl = remote.c_str();
    Cfg.talkRoute = talkRouteFromApiName(route.c_str(), Cfg.talkRoute);
    if (!token.isEmpty()) Cfg.hostToken = token.c_str();
    Cfg.firstRunComplete = !Cfg.hostAddr.empty() && !Cfg.hostToken.empty();
    Cfg.save();
    Sys.hostAddr = Cfg.hostAddr; Sys.hostPort = Cfg.hostPort; Sys.hostOnline = false;
    return "{\"ok\":true,\"result\":\"core_config_saved\"}";
}

void sendScreen(WiFiClient& c) {
    auto& canvas = shell::canvas();
    const uint8_t* pixels = static_cast<const uint8_t*>(canvas.getBuffer());
    if (!pixels) { sendText(c, 500, "application/json", "{\"ok\":false,\"error\":\"screen_unavailable\"}"); return; }
    sendHead(c, 200, "application/octet-stream", SCREEN_BYTES);
    c.write(pixels, SCREEN_BYTES);
}

void handleClient(WiFiClient& c) {
    c.setTimeout(350);
    String requestLine = c.readStringUntil('\n'); requestLine.trim();
    if (requestLine.length() == 0) return;
    const int sp1 = requestLine.indexOf(' '), sp2 = requestLine.indexOf(' ', sp1 + 1);
    if (sp1 < 1 || sp2 < 0) { sendText(c, 400, "text/plain", "bad request"); return; }
    const String method = requestLine.substring(0, sp1), path = requestLine.substring(sp1 + 1, sp2);

    size_t contentLength = 0; String token;
    while (c.connected()) {
        String line = c.readStringUntil('\n'); line.trim();
        if (line.length() == 0) break;
        if (line.startsWith("Content-Length:")) contentLength = line.substring(15).toInt();
        else if (line.startsWith("X-MAZ-Token:")) { token = line.substring(12); token.trim(); }
    }
    if (contentLength > MAX_BODY) { sendText(c, 413, "application/json", "{\"error\":\"body_too_large\"}"); return; }
    String body;
    if (contentLength) {
        body.reserve(contentLength);
        const uint32_t deadline = millis() + 800;
        while (body.length() < contentLength && millis() < deadline) {
            while (c.available() && body.length() < contentLength) body += static_cast<char>(c.read());
            delay(1);
        }
    }

    const bool unlocked = tokenOk(token);
    const bool setupProvision = net::setupApActive();
    if (method == "GET" && path == "/") { sendPage(c); return; }
    if (method == "GET" && path == "/api/status") { sendText(c, 200, "application/json", statusJson(unlocked)); return; }
    if (method == "GET" && path == "/api/screen") {
        if (!unlocked) { sendText(c, 401, "application/json", "{\"error\":\"unlock_required\"}"); return; }
        sendScreen(c); return;
    }
    if (method == "POST" && path == "/api/wifi") {
        if (!unlocked && !setupProvision) { sendText(c, 401, "application/json", "{\"error\":\"unlock_required\"}"); return; }
        sendText(c, 200, "application/json", saveWifi(body)); return;
    }
    if (method == "POST" && path == "/api/config") {
        if (!unlocked && !(setupProvision && Cfg.hostToken.empty())) { sendText(c, 401, "application/json", "{\"error\":\"unlock_required\"}"); return; }
        sendText(c, 200, "application/json", saveConfig(body)); return;
    }
    if (method == "POST" && path == "/api/action") {
        if (!unlocked) { sendText(c, 401, "application/json", "{\"error\":\"unlock_required\"}"); return; }
        sendText(c, 200, "application/json", performAction(body)); return;
    }
    sendText(c, 404, "application/json", "{\"error\":\"not_found\"}");
}

bool networkAvailable() { return net::connected() || net::setupApActive(); }

}  // namespace

void begin() {
    gRunning = false;
    gMdns = false;
}

void update() {
    if (networkAvailable() && !gRunning) {
        gServer.begin();
        gRunning = true;
        if (net::connected()) gMdns = MDNS.begin("mazpocket");
    } else if (!networkAvailable() && gRunning) {
        gServer.stop(); gRunning = false;
        if (gMdns) { MDNS.end(); gMdns = false; }
    } else if (net::connected() && gRunning && !gMdns) {
        gMdns = MDNS.begin("mazpocket");
    }

    if (gRunning) {
        WiFiClient c = gServer.available();
        if (c) { handleClient(c); c.stop(); }
    }

    if (gPending != Pending::None && millis() >= gPendingAt) {
        const Pending p = gPending; gPending = Pending::None;
        if (p == Pending::Launcher) launcher::reboot();
        else ESP.restart();
    }
}

bool running() { return gRunning; }

}  // namespace web
}  // namespace maz
