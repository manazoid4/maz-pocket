#include "control.h"

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

constexpr uint16_t PORT = 8022;

WiFiServer gServer(PORT);
WiFiClient gClient;
bool       gBound  = false;
bool       gAuthed = false;
String     gInbound;
String     gSerialInbound;

uint8_t keyCode(String name) {
    name.toUpperCase();
    if (name.length() == 1 && name[0] >= 'A' && name[0] <= 'Z')
        return KEY_A + (name[0] - 'A');
    if (name.length() == 1 && name[0] >= '1' && name[0] <= '9')
        return KEY_1 + (name[0] - '1');
    if (name == "0") return KEY_0;
    if (name == "TAB") return KEY_TAB;
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

void startServer() {
    if (gBound || WiFi.status() != WL_CONNECTED) return;
    gServer.begin();
    gServer.setNoDelay(true);
    gBound = true;
    Serial.printf("[net] control on %s:%u\n",
                  WiFi.localIP().toString().c_str(),
                  static_cast<unsigned>(PORT));
}

void consumeSerial() {
    // Serial.readStringUntil() uses Stream's timeout and can hold the foreground
    // loop when a sender delivers a partial line. Consume only bytes that are
    // already present so USB diagnostics can never pause the UI/audio loop.
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\n') {
            if (c != '\r' && gSerialInbound.length() < 240) gSerialInbound += c;
            continue;
        }
        const String line = gSerialInbound;
        gSerialInbound = "";
        const String reply = handleLine(line, true);
        if (!reply.isEmpty()) Serial.println(reply);
    }
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
        // Status is a snapshot, not a network operation. Older builds called
        // host::health()/assurance() here, which could block this foreground
        // control path for seconds when the PC disappeared.
        const char* nudge = Sys.agentQuestion ? "attention" :
                            (Sys.nudgeDue ? "due" : "clear");
        const unsigned agents = static_cast<unsigned>(Sys.agentsWorking) +
                                static_cast<unsigned>(Sys.agentsWaiting) +
                                static_cast<unsigned>(Sys.agentsStale);
        char out[300];
        snprintf(out, sizeof(out),
                 "MAZSTATUS wifi=%s host=%s link=%s nudge=%s agents=%u "
                 "heap=%lu minheap=%lu largest=%lu loopmax=%lu pressure=%u stall=%u",
                 Sys.wifiConnected ? "online" : "offline",
                 Sys.hostOnline ? "online" : "offline", host::linkName(),
                 nudge, agents,
                 static_cast<unsigned long>(Sys.freeHeap),
                 static_cast<unsigned long>(Sys.minFreeHeap),
                 static_cast<unsigned long>(Sys.largestFreeBlock),
                 static_cast<unsigned long>(Sys.loopMaxMs),
                 Sys.memoryPressure ? 1u : 0u,
                 Sys.uiStallObserved ? 1u : 0u);
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
            event.ch = text[i];
            event.down = true;
            shell::dispatchKey(event);
        }
        return String("MAZTYPE OK chars=") + text.length() + " screen=" +
               shell::currentId();
    }

    if (line.startsWith("MAZKEY\t")) {
        const int split = line.indexOf('\t', 7);
        if (split < 0) return "MAZKEY ERR fields";
        const uint8_t code = keyCode(line.substring(7, split));
        const String state = line.substring(split + 1);
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

    if (line.startsWith("MAZCOREPAIR\t")) {
        String fields[3];
        for (int i = 0; i < 3; ++i) fields[i] = field(line, 12, i);
        if (fields[0].isEmpty() || fields[2].isEmpty())
            return "MAZCOREPAIR ERR fields";

        const long parsedPort = fields[1].toInt();
        if (parsedPort < 1 || parsedPort > 65535)
            return "MAZCOREPAIR ERR port";

        Cfg.hostAddr = fields[0].c_str();
        Cfg.hostPort = static_cast<uint16_t>(parsedPort);
        Cfg.hostToken = fields[2].c_str();
        Cfg.firstRunComplete = true;
        Cfg.save();
        Sys.hostAddr = Cfg.hostAddr;
        Sys.hostPort = Cfg.hostPort;
        return String("MAZCOREPAIR OK host=") + Cfg.hostAddr.c_str() +
               " port=" + String(Cfg.hostPort);
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
        Cfg.hostToken = fields[4].c_str();
        Cfg.firstRunComplete = true;
        Cfg.save();
        net::begin();
        const bool connected = net::connect(Cfg.wifiSsid, Cfg.wifiPass);
        return String("MAZPAIR OK wifi=") +
               (connected ? "connected" : "saved") + " host=" +
               Cfg.hostAddr.c_str();
    }

    if (line.startsWith("MAZREMOTE\t")) {
        String remote = line.substring(10);
        remote.trim();
        if (!remote.isEmpty() && !remote.startsWith("https://"))
            return "MAZREMOTE ERR https_required";
        Cfg.hostRemoteUrl = remote.c_str();
        Cfg.save();
        return String("MAZREMOTE OK ") + (remote.isEmpty() ? "disabled" : "enabled");
    }

    return "";
}

void begin() { startServer(); }

void update() {
    startServer();
    consumeSerial();

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
            if (c != '\r' && gInbound.length() < 240) gInbound += c;
            continue;
        }
        String line = gInbound;
        gInbound = "";
        line.trim();
        if (line.isEmpty()) continue;

        if (!gAuthed) {
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
