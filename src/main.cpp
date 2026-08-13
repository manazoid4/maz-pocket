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
#include "core/shell.h"
#include "core/sys.h"
#include "input/keyboard.h"
#include "net/net.h"
#include "storage/store.h"

using namespace maz;

void setup() {
    auto cfg          = M5.config();
    cfg.internal_spk  = true;
    cfg.internal_mic  = true;
    cfg.internal_imu  = false;  // BMI270 is unused in v0.1; skip the probe
    cfg.clear_display = true;
    M5.begin(cfg);
    M5.Display.setRotation(1);

    Serial.begin(115200);
    ESP_LOGI("maz", "MAZ Pocket v%s starting on board id %d", MAZ_POCKET_VERSION,
             (int)M5.getBoard());

    Sys.bootMillis = millis();

    // Order matters: settings first (brightness/volume), then the pieces that
    // read them. Each failure is reported rather than silently tolerated.
    Cfg.load();

    if (!KB.begin())
        ESP_LOGE("maz", "keyboard did not init - is this really a Cardputer ADV?");

    store::begin();
    voice::begin();
    net::begin();

    // Restore a plausible clock so files stamped before any NTP sync are at
    // least ordered correctly. Sys.timeValid stays false until a real sync.
    if (Cfg.lastKnownEpoch > 0) {
        timeval tv;
        tv.tv_sec  = static_cast<time_t>(Cfg.lastKnownEpoch);
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
    }

    if (!shell::begin()) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_RED);
        M5.Display.drawString("MAZ Pocket: out of memory", 10, 60);
        return;
    }

    if (!KB.ok())
        notify::post(Note::Error, "Keyboard not found",
                     "TCA8418 did not answer");
    if (!store::ready())
        notify::post(Note::Warn, "No storage",
                     "notes and recordings are disabled");
}

void loop() {
    shell::loop();
    // A short yield keeps the watchdog happy and the radio serviced without
    // making input feel laggy.
    delay(2);
}
