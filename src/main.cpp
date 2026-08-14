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
}

void loop() {
    handlePairingCommand();
    shell::loop();
    // A short yield keeps the watchdog happy and the radio serviced without
    // making input feel laggy.
    delay(2);
}
