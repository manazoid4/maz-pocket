// MAZ Pocket local control plane.
// Firmware flashing deliberately does NOT live here. Keeping install/repair in
// the browser flasher makes this service small enough for Launcher installs and
// prevents a generic OTA routine from selecting somebody else's app partition.
#include "web.h"

#include <ESPmDNS.h>
#include <M5Unified.h>
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

constexpr uint16_t PORT = 80;
constexpr size_t MAX_BODY = 1100;
constexpr const char* FLASHER_URL =
    "https://mazos-site.vercel.app/maz-pocket/flasher/";

WiFiServer gServer(PORT);
bool gRunning = false;
bool gMdns = false;
uint32_t gReconnectAt = 0;
uint32_t gMicTestUntil = 0;

enum class Pending : uint8_t { None, Reboot, Launcher };
Pending gPending = Pending::None;
uint32_t gPendingAt = 0;

class DiscardSink final : public voice::Sink {
public:
    bool open() override { return true; }
    bool write(const int16_t*, size_t) override { return true; }
    bool close() override { return true; }
    const char* name() const override { return "web-test"; }
};
DiscardSink gMicSink;

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><title>MAZ Pocket</title><style>
:root{color-scheme:dark;--b:#0b0d10;--p:#15191e;--l:#30363d;--t:#edf0f2;--d:#9da7b1;--a:#ff7a18;--g:#40d982;--r:#ff5a5f}*{box-sizing:border-box}body{margin:0;background:var(--b);color:var(--t);font:14px ui-monospace,Consolas,monospace}main{max-width:780px;margin:auto;padding:16px}h1{font-size:21px;margin:4px 0}header{border-bottom:1px solid var(--l);padding-bottom:11px;margin-bottom:12px}.a{color:var(--a)}.d{color:var(--d)}.g{color:var(--g)}.r{color:var(--r)}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(235px,1fr));gap:9px}.c{background:var(--p);border:1px solid var(--l);border-radius:8px;padding:12px}.c h2{font-size:11px;letter-spacing:.12em;color:var(--a);margin:0 0 10px}.kv{display:grid;grid-template-columns:1fr auto;gap:6px}input,select,button,a.btn{font:inherit;border:1px solid var(--l);border-radius:6px;padding:8px;background:#0a0c0e;color:var(--t)}input,select{width:100%;margin:4px 0 8px}button,a.btn{display:inline-block;margin:3px 2px 3px 0;text-decoration:none;cursor:pointer}.primary{background:var(--a)!important;color:#08090a!important;border-color:var(--a)!important;font-weight:700}.meter{height:7px;border:1px solid var(--l);margin-top:7px}.meter i{height:100%;display:block;width:0;background:#29cbe8}pre{white-space:pre-wrap;color:var(--d);min-height:30px;margin:0}</style></head><body><main>
<header><h1><span class=a>MAZ</span> POCKET</h1><div class=d>mazpocket.local / status + control</div></header><div class=grid>
<section class=c><h2>DEVICE</h2><div class=kv><span>Version</span><b id=v>-</b><span>Battery</span><b id=bat>-</b><span>Storage</span><b id=st>-</b><span>Free</span><b id=free>-</b><span>SD</span><b id=sd>-</b></div></section>
<section class=c><h2>NETWORK</h2><div class=kv><span>Wi-Fi</span><b id=wifi>-</b><span>IP</span><b id=ip>-</b><span>Signal</span><b id=rssi>-</b><span>MAZ Host</span><b id=host>-</b><span>Agents</span><b id=agents>-</b></div></section>
<section class=c><h2>PHONE ACCESS</h2><input id=tok type=password placeholder="MAZ pairing token"><button class=primary onclick=unlock()>UNLOCK CONTROLS</button><div id=lock class=d>Read-only until unlocked.</div></section>
<section class=c><h2>SELF TEST</h2><button onclick="act('speaker')">SPEAKER</button><button onclick="act('mic')">MIC 4 SEC</button><button onclick="act('host')">HOST</button><button onclick="act('storage')">SD CHECK</button><div class=meter><i id=meter></i></div><small id=mic class=d>mic idle</small></section>
<section class=c><h2>CONFIG</h2><input id=ha placeholder="MAZ Host IP/name"><input id=hp type=number value=8787 placeholder="Host port"><input id=remote placeholder="Remote https://..."><select id=route><option value=0>LOCAL</option><option value=1>AUTO</option><option value=2>CLOUD</option></select><label><input id=tts style="width:auto" type=checkbox> spoken replies</label><br><button onclick=save()>SAVE</button></section>
<section class=c><h2>FIRMWARE</h2><p class=d>Updates use the safe Web Serial flasher. Device-side OTA is intentionally not installed.</p><a class="btn primary" href=")HTML";
const char PAGE2[] PROGMEM = R"HTML(" target=_blank rel=noopener>OPEN MAZ FLASHER</a></section>
<section class=c><h2>RECOVERY</h2><button onclick="act('reboot')">REBOOT</button><button onclick="act('launcher')">M5LAUNCHER</button></section>
<section class=c><h2>LOG</h2><pre id=log>Live status loads automatically.</pre></section></div></main><script>
const $=x=>document.getElementById(x);let T=sessionStorage.getItem('mazToken')||'';if(T)$('tok').value=T;const H=()=>T?{'X-MAZ-Token':T}:{};function L(x){$('log').textContent=x}function mb(n){return n?Math.round(n/1048576*10)/10+' MB':'-'}async function R(fill=false){try{let r=await fetch('/api/status',{headers:H()});let d=await r.json();$('v').textContent=d.version||'-';$('bat').textContent=d.battery<0?'?':d.battery+'%';$('st').textContent=d.storage||'-';$('free').textContent=mb(d.free);$('sd').innerHTML=d.sd_unreadable?'<span class=r>UNREADABLE</span>':d.sd_present?'<span class=g>READY</span>':'none';$('wifi').textContent=d.wifi_ssid||'offline';$('ip').textContent=d.ip||'-';$('rssi').textContent=d.rssi?d.rssi+' dBm':'-';$('host').textContent=d.host_link||'-';$('agents').textContent=(d.agents_working||0)+' work / '+(d.agents_waiting||0)+' wait';$('meter').style.width=Math.round((d.mic_level||0)*100)+'%';$('mic').textContent=d.mic_test?'mic live':'mic idle';$('lock').innerHTML=d.unlocked?'<span class=g>CONTROLS UNLOCKED</span>':'Read-only until unlocked.';if(fill&&d.unlocked){$('ha').value=d.host_addr||'';$('hp').value=d.host_port||8787;$('remote').value=d.remote_url||'';$('route').value=String(d.route||0);$('tts').checked=!!d.tts}}catch(e){L('Device page lost connection; Wi-Fi may be reconnecting.')}}async function unlock(){T=$('tok').value.trim();sessionStorage.setItem('mazToken',T);await R(true)}async function act(a){let r=await fetch('/api/action',{method:'POST',headers:{...H(),'Content-Type':'application/x-www-form-urlencoded'},body:'action='+encodeURIComponent(a)});L(await r.text());setTimeout(()=>R(false),250)}async function save(){let b=new URLSearchParams({host_addr:$('ha').value.trim(),host_port:$('hp').value||'8787',remote_url:$('remote').value.trim(),route:$('route').value,tts:$('tts').checked?'1':'0'});let r=await fetch('/api/config',{method:'POST',headers:{...H(),'Content-Type':'application/x-www-form-urlencoded'},body:b});L(await r.text());setTimeout(()=>R(true),400)}R(true);setInterval(()=>R(false),3000);
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
    c.print("HTTP/1.1 ");
    c.print(code);
    c.print(code == 200 ? " OK\r\n" : code == 401 ? " Unauthorized\r\n" : " Error\r\n");
    c.print("Content-Type: "); c.print(type);
    c.print("\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ");
    c.print(len); c.print("\r\n\r\n");
}

void sendText(WiFiClient& c, int code, const char* type, const String& body) {
    sendHead(c, code, type, body.length());
    c.print(body);
}

void sendPage(WiFiClient& c) {
    const size_t n1 = strlen(PAGE), nu = strlen(FLASHER_URL), n2 = strlen(PAGE2);
    sendHead(c, 200, "text/html; charset=utf-8", n1 + nu + n2);
    c.print(PAGE); c.print(FLASHER_URL); c.print(PAGE2);
}

String statusJson(bool unlocked) {
    String j;
    j.reserve(720);
    j += "{\"ok\":true,\"version\":\"" MAZ_POCKET_VERSION "\"";
    j += ",\"unlocked\":"; j += unlocked ? "true" : "false";
    j += ",\"battery\":" + String(Sys.batteryPct);
    j += ",\"charging\":"; j += Sys.charging ? "true" : "false";
    j += ",\"storage\":\"" + String(store::backendName()) + "\"";
    j += ",\"free\":" + String(static_cast<unsigned long long>(store::freeBytes()));
    j += ",\"total\":" + String(static_cast<unsigned long long>(store::totalBytes()));
    j += ",\"sd_present\":"; j += Sys.sdPresent ? "true" : "false";
    j += ",\"sd_unreadable\":"; j += Sys.sdUnreadable ? "true" : "false";
    j += ",\"wifi_ssid\":\"" + jsonEscape(Sys.wifiSsid.c_str()) + "\"";
    j += ",\"ip\":\"" + jsonEscape(Sys.ip.c_str()) + "\"";
    j += ",\"rssi\":" + String(net::rssi());
    j += ",\"host_link\":\"" + jsonEscape(host::linkName()) + "\"";
    j += ",\"agents_working\":" + String(Sys.agentsWorking);
    j += ",\"agents_waiting\":" + String(Sys.agentsWaiting);
    j += ",\"mic_test\":"; j += gMicTestUntil ? "true" : "false";
    j += ",\"mic_level\":" + String(voice::level(), 3);
    if (unlocked) {
        j += ",\"host_addr\":\"" + jsonEscape(Cfg.hostAddr.c_str()) + "\"";
        j += ",\"host_port\":" + String(Cfg.hostPort);
        j += ",\"remote_url\":\"" + jsonEscape(Cfg.hostRemoteUrl.c_str()) + "\"";
        j += ",\"route\":" + String(Cfg.talkRoute);
        j += ",\"tts\":"; j += Cfg.ttsEnabled ? "true" : "false";
    }
    j += '}';
    return j;
}

String storageCheck() {
    if (Sys.storage != Storage::SD) return "{\"ok\":false,\"error\":\"sd_not_active\"}";
    const std::string path = "/maz/cache/.web-sd-check";
    const std::string probe = "MAZ-SD-CHECK-0123456789";
    if (!store::writeText(path, probe)) return "{\"ok\":false,\"error\":\"sd_write_failed\"}";
    const std::string got = store::readText(path, 64);
    store::remove(path);
    return got == probe ? "{\"ok\":true,\"result\":\"sd_read_write_ok\"}"
                        : "{\"ok\":false,\"error\":\"sd_verify_failed\"}";
}

String action(const String& body) {
    const String name = formValue(body, "action");
    if (name == "speaker") {
        if (voice::state() == voice::State::Listening) return "{\"ok\":false,\"error\":\"mic_busy\"}";
        sfx::lineOpen();
        return "{\"ok\":true,\"result\":\"speaker_ok\"}";
    }
    if (name == "mic") {
        if (voice::state() != voice::State::Idle) return "{\"ok\":false,\"error\":\"audio_busy\"}";
        if (!voice::start(&gMicSink, 4)) return String("{\"ok\":false,\"error\":\"") + jsonEscape(voice::lastError()) + "\"}";
        gMicTestUntil = millis() + 4000;
        return "{\"ok\":true,\"result\":\"mic_test_started\"}";
    }
    if (name == "host")
        return host::health() ? "{\"ok\":true,\"result\":\"host_online\"}"
                              : "{\"ok\":false,\"error\":\"host_offline\"}";
    if (name == "storage") return storageCheck();
    if (name == "reboot" || name == "launcher") {
        gPending = name == "reboot" ? Pending::Reboot : Pending::Launcher;
        gPendingAt = millis() + 650;
        return name == "reboot" ? "{\"ok\":true,\"result\":\"rebooting\"}"
                                 : "{\"ok\":true,\"result\":\"launcher_handoff\"}";
    }
    return "{\"ok\":false,\"error\":\"action_not_allowed\"}";
}

String saveConfig(const String& body) {
    const String addr = formValue(body, "host_addr");
    const int port = formValue(body, "host_port").toInt();
    const String remote = formValue(body, "remote_url");
    const int route = formValue(body, "route").toInt();
    if (addr.isEmpty() || port < 1 || port > 65535) return "{\"ok\":false,\"error\":\"invalid_host\"}";
    if (!remote.isEmpty() && !remote.startsWith("https://")) return "{\"ok\":false,\"error\":\"remote_requires_https\"}";
    if (route < 0 || route > 2) return "{\"ok\":false,\"error\":\"invalid_route\"}";
    Cfg.hostAddr = addr.c_str();
    Cfg.hostPort = static_cast<uint16_t>(port);
    Cfg.hostRemoteUrl = remote.c_str();
    Cfg.talkRoute = static_cast<uint8_t>(route);
    Cfg.ttsEnabled = formValue(body, "tts") == "1";
    Cfg.save();
    Sys.hostAddr = Cfg.hostAddr;
    Sys.hostPort = Cfg.hostPort;
    return "{\"ok\":true,\"result\":\"saved\"}";
}

bool readLine(WiFiClient& c, String& out, uint32_t deadline) {
    out = "";
    while (millis() < deadline && c.connected()) {
        while (c.available()) {
            const char ch = static_cast<char>(c.read());
            if (ch == '\n') { out.trim(); return true; }
            if (ch != '\r' && out.length() < 512) out += ch;
        }
        delay(1);
    }
    return false;
}

void handle(WiFiClient c) {
    c.setNoDelay(true);
    const uint32_t deadline = millis() + 350;
    String line;
    if (!readLine(c, line, deadline)) { c.stop(); return; }
    const int s1 = line.indexOf(' '), s2 = line.indexOf(' ', s1 + 1);
    if (s1 < 1 || s2 < 0) { c.stop(); return; }
    const String method = line.substring(0, s1);
    const String path = line.substring(s1 + 1, s2);
    String token;
    size_t contentLength = 0;
    while (readLine(c, line, deadline) && !line.isEmpty()) {
        if (line.startsWith("X-MAZ-Token:")) { token = line.substring(12); token.trim(); }
        else if (line.startsWith("Content-Length:")) { String n = line.substring(15); n.trim(); contentLength = n.toInt(); }
    }
    if (contentLength > MAX_BODY) { sendText(c, 400, "application/json", "{\"ok\":false,\"error\":\"body_too_large\"}"); c.stop(); return; }
    String body;
    body.reserve(contentLength);
    while (body.length() < contentLength && millis() < deadline && c.connected()) {
        while (c.available() && body.length() < contentLength) body += static_cast<char>(c.read());
        delay(1);
    }

    if (method == "GET" && path == "/") sendPage(c);
    else if (method == "GET" && path.startsWith("/api/status"))
        sendText(c, 200, "application/json", statusJson(tokenOk(token)));
    else if ((path == "/api/action" || path == "/api/config") && !tokenOk(token))
        sendText(c, 401, "application/json", "{\"ok\":false,\"error\":\"pairing_token_required\"}");
    else if (method == "POST" && path == "/api/action")
        sendText(c, 200, "application/json", action(body));
    else if (method == "POST" && path == "/api/config")
        sendText(c, 200, "application/json", saveConfig(body));
    else sendText(c, 404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
    delay(1);
    c.stop();
}

void start() {
    if (gRunning || WiFi.status() != WL_CONNECTED) return;
    gServer.begin();
    gServer.setNoDelay(true);
    gRunning = true;
    if (!gMdns && MDNS.begin("mazpocket")) {
        MDNS.addService("http", "tcp", PORT);
        gMdns = true;
    }
    Serial.printf("[web] http://mazpocket.local ip=%s\n", WiFi.localIP().toString().c_str());
}

}  // namespace

void begin() { start(); }

void update() {
    if (WiFi.status() == WL_CONNECTED) {
        start();
        if (gRunning) {
            WiFiClient c = gServer.available();
            if (c) handle(c);
        }
    }
    if (gMicTestUntil && (millis() >= gMicTestUntil || voice::state() != voice::State::Listening)) {
        if (voice::state() == voice::State::Listening) voice::stop();
        gMicTestUntil = 0;
    }
    if (gReconnectAt && millis() >= gReconnectAt) {
        gReconnectAt = 0;
        net::begin();
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
