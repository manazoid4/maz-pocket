// MAZ Pocket — firmware entry point for the M5Stack Cardputer ADV.
//
// Everything real happens in core/shell; this file only brings the hardware
// up in the right order and says loudly when a subsystem does not answer.
#include <M5Unified.h>
#include <sys/time.h>
#include <time.h>

#include "audio/voice.h"
#include "core/notify.h"
#include "core/settings.h"
#include "core/launcher.h"
#include "core/shell.h"
#include "core/sys.h"
#include "input/keyboard.h"
#include "net/mazhost.h"
#include "net/net.h"
#include "storage/store.h"

using namespace maz;

namespace {
uint8_t serialKeyCode(String name) {
    name.toUpperCase();
    if (name.length() == 1 && name[0] >= 'A' && name[0] <= 'Z')
        return KEY_A + (name[0] - 'A');
    // Home pages with TAB and opens cells by their digit badge, so the
    // acceptance harness has to be able to send both. Without these the whole
    // paging path is undrivable over USB and simply looks like it does nothing.
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

void handlePairingCommand() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line == "MAZSTATUS") {
        const bool hostOnline = host::health();
        const host::Assurance fleet = hostOnline ? host::assurance() : host::Assurance{};
        Serial.printf("MAZSTATUS wifi=%s host=%s nudge=%s agents=%u\n",
                      Sys.wifiConnected ? "online" : "offline",
                      hostOnline ? "online" : "offline", fleet.state.c_str(),
                      static_cast<unsigned>(fleet.agents.size()));
        return;
    }
    if (line == "MAZSCREEN") {
        const bool recording = voice::state() == voice::State::Listening ||
                               voice::state() == voice::State::Paused;
        Serial.printf(
            "MAZSCREEN screen=%s recording=%u focus=%u storage=%s "
            "sdbad=%u wiped=%u braindumps=%u "
            "inbox=%u decisions=%u reminders=%u sprints=%u\n",
            shell::currentId(), recording ? 1 : 0,
            shell::focus::running() ? 1 : 0, store::backendName(),
            Sys.sdUnreadable ? 1 : 0, Sys.internalFormatted ? 1 : 0,
            static_cast<unsigned>(store::list("braindumps", "wav", 1000).size()),
            static_cast<unsigned>(store::loadRecords("inbox", 1000).size()),
            static_cast<unsigned>(store::loadRecords("decision", 1000).size()),
            static_cast<unsigned>(store::loadRecords("reminder", 1000).size()),
            static_cast<unsigned>(store::loadRecords("sprint", 1000).size()));
        return;
    }
    if (line.startsWith("MAZOPEN\t")) {
        const String target = line.substring(8);
        shell::goHome();
        const bool ok = target == "home" || shell::pushById(target.c_str());
        Serial.printf("MAZOPEN %s screen=%s\n", ok ? "OK" : "ERR",
                      shell::currentId());
        return;
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
        Serial.printf("MAZTYPE OK chars=%u screen=%s\n",
                      static_cast<unsigned>(text.length()), shell::currentId());
        return;
    }
    if (line.startsWith("MAZKEY\t")) {
        const int split = line.indexOf('\t', 7);
        if (split < 0) { Serial.println("MAZKEY ERR fields"); return; }
        const uint8_t code = serialKeyCode(line.substring(7, split));
        const String state = line.substring(split + 1);
        if (code == KEY_NONE || (state != "DOWN" && state != "UP")) {
            Serial.println("MAZKEY ERR key");
            return;
        }
        KeyEvent event;
        event.code = code;
        event.down = state == "DOWN";
        shell::dispatchKey(event);
        Serial.printf("MAZKEY OK screen=%s\n", shell::currentId());
        return;
    }
    if (line == "MAZLAUNCHER") {
        Serial.println("MAZLAUNCHER OK");
        delay(100);
        if (!launcher::reboot()) Serial.println("MAZLAUNCHER ERR handback");
        return;
    }
    if (!line.startsWith("MAZPAIR\t")) return;

    String fields[5];
    int start = 8;
    for (int i = 0; i < 5; ++i) {
        const int tab = line.indexOf('\t', start);
        fields[i] = tab < 0 ? line.substring(start) : line.substring(start, tab);
        start = tab < 0 ? line.length() : tab + 1;
    }
    if (fields[0].isEmpty() || fields[2].isEmpty() || fields[4].isEmpty()) {
        Serial.println("MAZPAIR ERR fields");
        return;
    }

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
    Serial.printf("MAZPAIR OK wifi=%s host=%s\n",
                  connected ? "connected" : "saved", Cfg.hostAddr.c_str());
}
}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("[boot] serial");

    auto cfg          = M5.config();
    cfg.internal_spk  = true;
    cfg.internal_mic  = true;
    cfg.internal_imu  = false;  // BMI270 is unused in v0.1; skip the probe
    cfg.clear_display = true;
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setBrightness(180);
    Serial.printf("[boot] display board=%d\n", (int)M5.getBoard());

    Sys.bootMillis = millis();

    // Order matters: settings first (brightness/volume), then the pieces that
    // read them. Each failure is reported rather than silently tolerated.
    Cfg.load();
    Serial.println("[boot] settings");

    if (!KB.begin())
        ESP_LOGE("maz", "keyboard did not init - is this really a Cardputer ADV?");
    Serial.printf("[boot] keyboard=%s\n", KB.ok() ? "ok" : "missing");

    store::begin();
    Serial.printf("[boot] storage=%s\n", store::backendName());
    voice::begin();
    Serial.println("[boot] audio");
    net::begin();
    Serial.println("[boot] network");

    // Restore a plausible clock so files stamped before any NTP sync are at
    // least ordered correctly. Sys.timeValid stays false until a real sync.
    if (Cfg.lastKnownEpoch > 0) {
        timeval tv;
        tv.tv_sec  = static_cast<time_t>(Cfg.lastKnownEpoch);
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
    }

    if (!shell::begin()) {
        Serial.println("[boot] shell failed: out of memory");
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_RED);
        M5.Display.drawString("MAZ Pocket: out of memory", 10, 60);
        return;
    }
    Serial.println("[boot] shell");

    Serial.printf("MAZ Pocket %s READY board=%d keyboard=%s storage=%s\n",
                  MAZ_POCKET_VERSION, (int)M5.getBoard(),
                  KB.ok() ? "ok" : "missing", store::backendName());

    if (!KB.ok())
        notify::post(Note::Error, "Keyboard not found",
                     "TCA8418 did not answer");
    if (!store::ready())
        notify::post(Note::Warn, "No storage",
                     "notes and recordings are disabled");
    // Both of these used to be log-only, so the device looked healthy while
    // silently running on volatile storage, or having just erased itself.
    else if (Sys.internalFormatted)
        notify::post(Note::Warn, "Internal storage reset",
                     "previous notes and recordings are gone");
    if (Sys.sdUnreadable)
        notify::post(Note::Warn, "SD card unreadable",
                     "format it as FAT32 to keep data safely");
}

void loop() {
    handlePairingCommand();
    shell::loop();
    // A short yield keeps the watchdog happy and the radio serviced without
    // making input feel laggy.
    delay(2);
}
