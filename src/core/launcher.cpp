#include "launcher.h"

#include <Arduino.h>
#include <esp_ota_ops.h>

namespace maz::launcher {
namespace {

// An app partition is bootable only if it starts with the ESP image magic.
// Checking this before changing the boot target avoids sending the device to an
// empty/corrupt slot.
bool bootable(const esp_partition_t* part) {
    if (!part) return false;
    uint8_t magic = 0;
    if (esp_partition_read(part, 0, &magic, sizeof(magic)) != ESP_OK)
        return false;
    return magic == 0xE9;
}

// M5Launcher normally lives in its TEST app partition. Keep fallbacks for
// devices whose Launcher image sits in factory or another valid app slot.
const esp_partition_t* handBackTarget(const esp_partition_t* running) {
    const esp_partition_t* test = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr);
    if (test && (!running || test->address != running->address) && bootable(test))
        return test;

    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
    if (factory && (!running || factory->address != running->address) && bootable(factory))
        return factory;

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
    if (!running || running->type != ESP_PARTITION_TYPE_APP) return false;

    const esp_partition_t* target = handBackTarget(running);
    if (!target) {
        ESP_LOGE("launcher",
                 "no bootable M5Launcher/fallback partition found - refusing "
                 "to change boot state");
        return false;
    }

    // IMPORTANT: never invalidate/erase the currently running MAZ Pocket image
    // to get back to M5Launcher. Older builds overwrote the first four bytes of
    // the running app, which made the MAZ slot disappear and forced an SD
    // reinstall every time the user entered Launcher. Select the already-valid
    // Launcher partition as the next boot target instead; MAZ Pocket remains a
    // valid app that M5Launcher can start again immediately.
    const esp_err_t result = esp_ota_set_boot_partition(target);
    if (result != ESP_OK) {
        ESP_LOGE("launcher", "could not select hand-back partition: %s",
                 esp_err_to_name(result));
        return false;
    }

    ESP_LOGI("launcher", "returning to partition %s at 0x%08lx without erasing MAZ",
             target->label, static_cast<unsigned long>(target->address));
    ESP.restart();
    return true;
}

}  // namespace maz::launcher
