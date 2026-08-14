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

// Where control would actually land once this app invalidates itself. The
// bootloader prefers the TEST partition M5Launcher installs itself into, then
// a factory image, then any other slot carrying a real image.
const esp_partition_t* handBackTarget(const esp_partition_t* running) {
    const esp_partition_t* test = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr);
    if (bootable(test)) return test;

    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
    if (bootable(factory)) return factory;

    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    const esp_partition_t* found = nullptr;
    while (it) {
        const esp_partition_t* candidate = esp_partition_get(it);
        if (candidate && (!running || candidate->address != running->address) &&
            bootable(candidate)) {
            found = candidate;
            break;
        }
        it = esp_partition_next(it);
    }
    if (it) esp_partition_iterator_release(it);
    return found;
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
    if (!handBackTarget(running)) {
        ESP_LOGE("launcher",
                 "no bootable partition to hand back to - refusing to "
                 "invalidate the only working image");
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
