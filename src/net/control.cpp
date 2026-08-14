#include "control.h"

#include <ArduinoOTA.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "../audio/voice.h"
#include "../core/launcher.h"
#include "../core/settings.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../storage/store.h"
#include "mazhost.h"
#include "net.h"

namespace maz {
namespace control {
namespace {

// Same language as the USB surface, on a port that is deliberately not 80:
// this is a control channel for the owner's laptop, not a web page.
constexpr uint16_t PORT = 8022;

WiFiServer gServer(PORT);
WiFiClient gClient;
bool       gBound  = false;
bool       gOtaUp  = false;
bool       gAuthed = false;
String     gInbound;

uint8_t keyCode(String name) {
    name.toUpperCase();
    if (name.length() == 1 && name[0] >= 'A' && name[0] <= 'Z')
        return KEY_A + (name[0] - 'A');
    // Home pages with TAB and opens cells by their digit badge, so a harness
    // has to be able to send both. Without these the paging path is undrivable
    // and simply looks like it does nothing.
    if (name.length() == 1 && name[0] >= '1' && name[0] <= '9')
        return KEY_1 + (name[0] - '1');
    if (name == "0") return KEY_0;
    if (name == "TAB") return KEY_TAB;
    // The keys the arrows are printed on. The shell turns these into UP/LEFT/
    // DOWN/RIGHT/ESC when the focused app does not want the character, so the
    // harness has to be able to send the raw key to prove that path works.
    if (name == "SEMICOLON") return KEY_SEMICOLON;
    if (name == "COMMA") return KEY_COMMA;
    if (name == "DOT") return KEY_DOT;
    if (name == "SLASH") return KEY_SLASH;
    if (name == "GRAVE") return KEY_GRAVE;
    if (name == "ENTER") return KEY_ENTER;
    if (name == "ESC") return KEY_ESC;
    if (name == "SPACE") return KEY_SPACE;
    if (name == "UP") return KEY_UP;
    if (name == "DOWN") return KEY_DOWN;
    if (name == "LEFT") return KEY_LEFT;
    if (name == "RIGHT") return KEY_RIGHT;
    if (name == "BACKSPACE") return KEY_BACKSPACE;
    return KEY_NONE;
}

String field(const String& line, int from, int index) {
    int start = from;
    for (int i = 0; i < index; ++i) {
        const int tab = line.indexOf('\t', start);
        if (tab < 0) return "";
        start = tab + 1;
    }
    const int tab = line.indexOf('\t', start);
    return tab < 0 ? line.substring(start) : line.substring(start, tab);
}

void startOta() {
    if (gOtaUp || WiFi.status() != WL_CONNECTED) return;
    ArduinoOTA.setHostname("maz-pocket");
    // The pairing token is already a shared secret between this device and the
    // laptop. Reusing it means there is no second credential to lose, and an
    // unpaired device cannot be updated over the air at all.
    if (!Cfg.hostToken.empty()) ArduinoOTA.setPassword(Cfg.hostToken.c_str());
    ArduinoOTA.onStart([]() {
        // Audio DMA and an in-flight flash write must not overlap.
        if (voice::state() == voice::State::Listening) voice::stop();
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_ORANGE);
        M5.Display.drawString("Updating over Wi-Fi", 10, 50);
        M5.Display.drawString("do not power off", 10, 70);
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        if (!total) return;
        M5.Display.drawRect(10, 95, 220, 10, TFT_DARKGREY);
        M5.Display.fillRect(10, 95, (220 * done) / total, 10, TFT_ORANGE);
    });
    ArduinoOTA.onEnd([]() {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.drawString("Updated. Restarting.", 10, 60);
    });
    ArduinoOTA.begin();
    gOtaUp = true;
    Serial.printf("[net] OTA ready as maz-pocket at %s\n",
                  WiFi.localIP().toString().c_str());
}

void startServer() {
    if (gBound || WiFi.status() != WL_CONNECTED) return;
    gServer.begin();
    gServer.setNoDelay(true);
    gBound = true;
    Serial.printf("[net] control on %s:%u\n",
                  WiFi.localIP().toString().c_str(),
                  static_cast<unsigned>(PORT));
}

}  // namespace

bool listening() { return gBound; }

String handleLine(const String& raw, bool trusted) {
    String line = raw;
    line.trim();
    if (line.isEmpty()) return "";

    if (line == "MAZPING")
        return String("MAZPING OK version=" MAZ_POCKET_VERSION " ip=") +
               WiFi.localIP().toString();

    if (line == "MAZSTATUS") {
        const bool            hostOnline = host::health();
        const host::Assurance fleet =
            hostOnline ? host::assurance() : host::Assurance{};
        char out[160];
        snprintf(out, sizeof(out), "MAZSTATUS wifi=%s host=%s nudge=%s agents=%u",
                 Sys.wifiConnected ? "online" : "offline",
                 hostOnline ? "online" : "offline", fleet.state.c_str(),
                 static_cast<unsigned>(fleet.agents.size()));
        return out;
    }

    if (line == "MAZSCREEN") {
        const bool recording = voice::state() == voice::State::Listening ||
                               voice::state() == voice::State::Paused;
        char out[240];
        snprintf(out, sizeof(out),
                 "MAZSCREEN screen=%s recording=%u focus=%u storage=%s "
                 "sdbad=%u wiped=%u braindumps=%u inbox=%u decisions=%u "
                 "reminders=%u sprints=%u",
                 shell::currentId(), recording ? 1 : 0,
                 shell::focus::running() ? 1 : 0, store::backendName(),
                 Sys.sdUnreadable ? 1 : 0, Sys.internalFormatted ? 1 : 0,
                 static_cast<unsigned>(store::list("braindumps", "wav", 1000).size()),
                 static_cast<unsigned>(store::loadRecords("inbox", 1000).size()),
                 static_cast<unsigned>(store::loadRecords("decision", 1000).size()),
                 static_cast<unsigned>(store::loadRecords("reminder", 1000).size()),
                 static_cast<unsigned>(store::loadRecords("sprint", 1000).size()));
        return out;
    }

    // Everything past this point moves the device, so it needs the token.
    if (!trusted) return "MAZERR unauthorised";

    if (line.startsWith("MAZOPEN\t")) {
        const String target = line.substring(8);
        shell::goHome();
        const bool ok = target == "home" || shell::pushById(target.c_str());
        return String("MAZOPEN ") + (ok ? "OK" : "ERR") + " screen=" +
               shell::currentId();
    }

    if (line.startsWith("MAZTYPE\t")) {
        String text = line.substring(8);
        if (text.length() > 120) text.remove(120);
        for (size_t i = 0; i < text.length(); ++i) {
            KeyEvent event;
            event.ch   = text[i];
            event.down = true;
            shell::dispatchKey(event);
        }
        return String("MAZTYPE OK chars=") + text.length() + " screen=" +
               shell::currentId();
    }

    if (line.startsWith("MAZKEY\t")) {
        const int split = line.indexOf('\t', 7);
        if (split < 0) return "MAZKEY ERR fields";
        const uint8_t code  = keyCode(line.substring(7, split));
        const String  state = line.substring(split + 1);
        if (code == KEY_NONE || (state != "DOWN" && state != "UP"))
            return "MAZKEY ERR key";
        KeyEvent event;
        event.code = code;
        event.down = state == "DOWN";
        shell::dispatchKey(event);
        return String("MAZKEY OK screen=") + shell::currentId();
    }

    if (line == "MAZLAUNCHER") {
        delay(100);
        if (!launcher::reboot()) return "MAZLAUNCHER ERR handback";
        return "MAZLAUNCHER OK";
    }

    if (line.startsWith("MAZPAIR\t")) {
        String fields[5];
        for (int i = 0; i < 5; ++i) fields[i] = field(line, 8, i);
        if (fields[0].isEmpty() || fields[2].isEmpty() || fields[4].isEmpty())
            return "MAZPAIR ERR fields";

        Cfg.wifiSsid = fields[0].c_str();
        Cfg.wifiPass = fields[1].c_str();
        Cfg.hostAddr = fields[2].c_str();
        Cfg.hostPort = static_cast<uint16_t>(fields[3].toInt());
        if (!Cfg.hostPort) Cfg.hostPort = 8787;
        Cfg.hostToken        = fields[4].c_str();
        Cfg.firstRunComplete = true;
        Cfg.save();
        net::begin();
        const bool connected = net::connect(Cfg.wifiSsid, Cfg.wifiPass);
        return String("MAZPAIR OK wifi=") +
               (connected ? "connected" : "saved") + " host=" +
               Cfg.hostAddr.c_str();
    }

    return "";
}

void begin() {
    startServer();
    startOta();
}

void update() {
    // Wi-Fi comes up after boot and can drop, so both surfaces are armed here
    // rather than once in begin().
    startServer();
    startOta();
    if (gOtaUp) ArduinoOTA.handle();

    if (Serial.available()) {
        const String reply = handleLine(Serial.readStringUntil('\n'), true);
        if (!reply.isEmpty()) Serial.println(reply);
    }

    if (!gBound) return;
    if (!gClient || !gClient.connected()) {
        WiFiClient next = gServer.available();
        if (next) {
            gClient = next;
            gAuthed = false;
            gInbound = "";
            gClient.println("MAZ Pocket " MAZ_POCKET_VERSION " - MAZAUTH first");
        }
        return;
    }

    while (gClient.available()) {
        const char c = static_cast<char>(gClient.read());
        if (c != '\n') {
            if (c != '\r' && gInbound.length() < 200) gInbound += c;
            continue;
        }
        String line = gInbound;
        gInbound    = "";
        line.trim();
        if (line.isEmpty()) continue;

        if (!gAuthed) {
            // One shot: a wrong token drops the connection rather than letting
            // something sit on the port guessing.
            if (line.startsWith("MAZAUTH\t") && !Cfg.hostToken.empty() &&
                line.substring(8) == Cfg.hostToken.c_str()) {
                gAuthed = true;
                gClient.println("MAZAUTH OK");
            } else {
                gClient.println("MAZAUTH ERR");
                gClient.stop();
            }
            continue;
        }

        if (line == "MAZBYE") {
            gClient.println("MAZBYE OK");
            gClient.stop();
            continue;
        }
        const String reply = handleLine(line, true);
        gClient.println(reply.isEmpty() ? "MAZERR unknown" : reply);
    }
}

}  // namespace control
}  // namespace maz
