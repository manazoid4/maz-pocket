#include "launcher.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp32s3/rom/spi_flash.h>

namespace maz::launcher {
namespace {

// An app partition is bootable only if it starts with the ESP image magic.
// Checking this is the difference between handing control back and stranding
// the device in a bootloader loop that needs a cable to escape.
bool bootable(const esp_partition_t* part) {
    if (!part) return false;
    uint8_t magic = 0;
    if (esp_partition_read(part, 0, &magic, sizeof(magic)) != ESP_OK)
        return false;
    return magic == 0xE9;
}

// Where control would land once this app invalidates itself.
//
// Only a TEST or FACTORY image counts. M5Launcher installs itself into TEST,
// and "hand back to the launcher" has no meaning if the answer is another copy
// of MAZ Pocket sitting in the neighbouring OTA slot.
//
// Accepting any bootable slot is precisely what made this dangerous: the first
// press invalidated the running image and booted the other copy, so a second
// press invalidated that one too and the device was left with no valid app at
// all. Recovering from that needs a cable, so the guard has to make it
// unreachable rather than merely unlikely.
const esp_partition_t* handBackTarget() {
    const esp_partition_t* test = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr);
    if (bootable(test)) return test;

    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
    if (bootable(factory)) return factory;

    return nullptr;
}

}  // namespace

bool reboot() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running || running->type != ESP_PARTITION_TYPE_APP ||
        running->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MIN ||
        running->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
        return false;
    }

    // Refuse rather than destroy. The whole mechanism here is to make the
    // running image unbootable, so with nothing valid to fall into this does
    // not hand control back - it strands the device somewhere only a cable can
    // reach. A handheld should never be one keystroke away from that.
    if (!handBackTarget()) {
        ESP_LOGE("launcher",
                 "M5Launcher is not installed - refusing to invalidate this "
                 "image, which would leave nothing to boot");
        return false;
    }

    // Arduino-ESP32 2.x deliberately aborts high-level writes into the running
    // app. This terminal operation uses the ESP32-S3 ROM primitive for exactly
    // four bytes; no code is executed from flash after it returns.
    static DRAM_ATTR const uint32_t invalid = 0;
    noInterrupts();
    if (esp_rom_spiflash_write(running->address, &invalid, sizeof(invalid)) !=
        ESP_ROM_SPIFLASH_RESULT_OK) return false;

    ESP.restart();
    return true;
}

}  // namespace maz::launcher
