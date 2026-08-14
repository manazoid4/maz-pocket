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
#include "net/control.h"
#include "net/mazhost.h"
#include "net/net.h"
#include "storage/store.h"

using namespace maz;


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
    // The control surface answers on USB immediately and binds its Wi-Fi
    // listener as soon as there is an address to bind to.
    control::begin();

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
    control::update();
    shell::loop();
    // A short yield keeps the watchdog happy and the radio serviced without
    // making input feel laggy.
    delay(2);
}
