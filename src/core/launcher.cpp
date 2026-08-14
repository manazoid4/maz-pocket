#include "launcher.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp32s3/rom/spi_flash.h>

namespace maz::launcher {
bool reboot() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running || running->type != ESP_PARTITION_TYPE_APP ||
        running->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MIN ||
        running->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
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
