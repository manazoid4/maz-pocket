#include "safe_ota.h"

#include <Preferences.h>
#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <cstring>

namespace maz::safeota {
namespace {

constexpr const char* NS = "mazota";
constexpr uint32_t FNV_OFFSET = 2166136261u;
constexpr uint32_t FNV_PRIME = 16777619u;

struct PairState {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t layout = 0;
    String hashA;
    String hashB;
};

esp_ota_handle_t gHandle = 0;
const esp_partition_t* gTarget = nullptr;
const esp_partition_t* gRunning = nullptr;
size_t gExpected = 0;
size_t gWritten = 0;
PairState gPair;
Status gCachedStatus;
uint32_t gCachedAt = 0;
bool gCachedValid = false;

uint32_t fnv(uint32_t hash, const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= p[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

uint32_t layoutSignature() {
    uint32_t hash = FNV_OFFSET;
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it) {
        const esp_partition_t* p = esp_partition_get(it);
        if (p) {
            hash = fnv(hash, &p->type, sizeof(p->type));
            hash = fnv(hash, &p->subtype, sizeof(p->subtype));
            hash = fnv(hash, &p->address, sizeof(p->address));
            hash = fnv(hash, &p->size, sizeof(p->size));
            hash = fnv(hash, p->label, strnlen(p->label, 16));
        }
        it = esp_partition_next(it);
    }
    return hash;
}

bool otaApp(const esp_partition_t* p) {
    return p && p->type == ESP_PARTITION_TYPE_APP &&
           p->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
           p->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX;
}

const esp_partition_t* byAddress(uint32_t address) {
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    const esp_partition_t* found = nullptr;
    while (it) {
        const esp_partition_t* p = esp_partition_get(it);
        if (p && p->address == address) {
            found = p;
            break;
        }
        it = esp_partition_next(it);
    }
    if (it) esp_partition_iterator_release(it);
    return found;
}

bool blank(const esp_partition_t* p) {
    if (!otaApp(p)) return false;
    uint8_t bytes[64];
    if (esp_partition_read(p, 0, bytes, sizeof(bytes)) != ESP_OK) return false;
    for (uint8_t b : bytes) if (b != 0xFF) return false;
    return true;
}

String partitionHash(const esp_partition_t* p) {
    if (!p) return "";
    uint8_t digest[32];
    if (esp_partition_get_sha256(p, digest) != ESP_OK) return "";
    static const char hex[] = "0123456789abcdef";
    char out[65];
    for (size_t i = 0; i < sizeof(digest); ++i) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    out[64] = 0;
    return String(out);
}

PairState loadPair() {
    PairState pair;
    Preferences p;
    if (!p.begin(NS, true)) return pair;
    pair.a = p.getULong("a", 0);
    pair.b = p.getULong("b", 0);
    pair.layout = p.getULong("layout", 0);
    pair.hashA = p.getString("ha", "");
    pair.hashB = p.getString("hb", "");
    p.end();
    return pair;
}

void savePair(const PairState& pair) {
    Preferences p;
    if (!p.begin(NS, false)) return;
    p.putULong("a", pair.a);
    p.putULong("b", pair.b);
    p.putULong("layout", pair.layout);
    p.putString("ha", pair.hashA);
    p.putString("hb", pair.hashB);
    p.end();
}

void clearPair() {
    Preferences p;
    if (!p.begin(NS, false)) return;
    p.clear();
    p.end();
}

String& hashFor(PairState& pair, uint32_t address) {
    return address == pair.a ? pair.hashA : pair.hashB;
}

const String& hashFor(const PairState& pair, uint32_t address) {
    return address == pair.a ? pair.hashA : pair.hashB;
}

const esp_partition_t* findBlankPartner(
    const esp_partition_t* running, size_t required) {
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    const esp_partition_t* best = nullptr;
    while (it) {
        const esp_partition_t* p = esp_partition_get(it);
        if (otaApp(p) && p->address != running->address && p->size >= required && blank(p)) {
            if (!best || p->size < best->size) best = p;
        }
        it = esp_partition_next(it);
    }
    return best;
}

Status inspect(size_t firmwareSize, bool claim) {
    Status out;
    const esp_partition_t* running = esp_ota_get_running_partition();
    out.runningAddress = running ? running->address : 0;
    if (!otaApp(running)) {
        out.reason = "running_image_is_not_an_ota_slot";
        return out;
    }

    size_t required = firmwareSize;
    if (!required) required = ESP.getSketchSize();
    if (!required) required = 1;

    const uint32_t layout = layoutSignature();
    PairState pair = loadPair();
    bool pairPresent = pair.a && pair.b;

    if (pairPresent && pair.layout != layout) {
        clearPair();
        pair = PairState{};
        pairPresent = false;
    }

    if (pairPresent) {
        const bool runningIsA = running->address == pair.a;
        const bool runningIsB = running->address == pair.b;
        if (!runningIsA && !runningIsB) {
            clearPair();
            pair = PairState{};
            pairPresent = false;
        } else {
            const uint32_t targetAddress = runningIsA ? pair.b : pair.a;
            const esp_partition_t* target = byAddress(targetAddress);
            if (!otaApp(target)) {
                out.reason = "owned_partner_missing";
                return out;
            }
            out.targetAddress = target->address;
            out.targetSize = target->size;
            if (target->size < required) {
                out.reason = "owned_partner_too_small";
                return out;
            }

            const String expected = hashFor(pair, target->address);
            if (!expected.isEmpty()) {
                const String actual = partitionHash(target);
                if (actual.isEmpty() || actual != expected) {
                    out.reason = "owned_partner_changed";
                    return out;
                }
            }
            out.safe = true;
            out.reason = "ready_owned_pair";
            return out;
        }
    }

    const esp_partition_t* partner = findBlankPartner(running, required);
    if (!partner) {
        out.reason = "no_blank_ota_partner_usb_update_required";
        return out;
    }
    out.targetAddress = partner->address;
    out.targetSize = partner->size;
    out.safe = true;
    out.reason = "ready_blank_partner";

    if (claim) {
        PairState next;
        next.a = running->address;
        next.b = partner->address;
        next.layout = layout;
        next.hashA = partitionHash(running);
        next.hashB = "";
        if (next.hashA.isEmpty()) {
            out.safe = false;
            out.reason = "could_not_hash_running_image";
            return out;
        }
        savePair(next);
    }
    return out;
}

void failAndAbort(String& error, const char* reason) {
    error = reason;
    if (gHandle) esp_ota_abort(gHandle);
    gHandle = 0;
    gTarget = nullptr;
    gRunning = nullptr;
    gExpected = 0;
    gWritten = 0;
}

}  // namespace

Status status(size_t firmwareSize) {
    if (!firmwareSize && gCachedValid && millis() - gCachedAt < 300000UL)
        return gCachedStatus;
    Status value = inspect(firmwareSize, false);
    if (!firmwareSize) {
        gCachedStatus = value;
        gCachedAt = millis();
        gCachedValid = true;
    }
    return value;
}

bool begin(size_t firmwareSize, String& error) {
    abort();
    gCachedValid = false;
    if (!firmwareSize) {
        error = "firmware_size_missing";
        return false;
    }

    Status plan = inspect(firmwareSize, true);
    if (!plan.safe) {
        error = plan.reason;
        return false;
    }

    gRunning = esp_ota_get_running_partition();
    gTarget = byAddress(plan.targetAddress);
    if (!otaApp(gRunning) || !otaApp(gTarget) || gTarget->address == gRunning->address) {
        error = "ota_plan_changed";
        abort();
        return false;
    }

    gPair = loadPair();
    const esp_err_t rc = esp_ota_begin(gTarget, firmwareSize, &gHandle);
    if (rc != ESP_OK) {
        error = String("esp_ota_begin_") + esp_err_to_name(rc);
        abort();
        return false;
    }
    gExpected = firmwareSize;
    gWritten = 0;
    return true;
}

bool write(const uint8_t* data, size_t size, String& error) {
    if (!gHandle || !gTarget || !data || !size) {
        error = "ota_not_started";
        return false;
    }
    if (gWritten == 0 && data[0] != 0xE9) {
        failAndAbort(error, "invalid_esp_image_magic");
        return false;
    }
    if (gWritten + size > gExpected) {
        failAndAbort(error, "firmware_larger_than_declared");
        return false;
    }
    const esp_err_t rc = esp_ota_write(gHandle, data, size);
    if (rc != ESP_OK) {
        error = String("esp_ota_write_") + esp_err_to_name(rc);
        abort();
        return false;
    }
    gWritten += size;
    return true;
}

bool finish(String& error) {
    gCachedValid = false;
    if (!gHandle || !gTarget || !gRunning) {
        error = "ota_not_started";
        return false;
    }
    if (gWritten != gExpected) {
        failAndAbort(error, "firmware_size_mismatch");
        return false;
    }

    const esp_err_t endRc = esp_ota_end(gHandle);
    gHandle = 0;
    if (endRc != ESP_OK) {
        error = String("esp_ota_end_") + esp_err_to_name(endRc);
        gTarget = nullptr;
        gRunning = nullptr;
        return false;
    }

    String targetHash = partitionHash(gTarget);
    String runningHash = partitionHash(gRunning);
    if (targetHash.isEmpty() || runningHash.isEmpty()) {
        error = "firmware_hash_verification_failed";
        gTarget = nullptr;
        gRunning = nullptr;
        return false;
    }

    gPair = loadPair();
    if (!(gPair.a && gPair.b) || gPair.layout != layoutSignature()) {
        error = "ota_ownership_state_changed";
        gTarget = nullptr;
        gRunning = nullptr;
        return false;
    }
    hashFor(gPair, gTarget->address) = targetHash;
    hashFor(gPair, gRunning->address) = runningHash;
    savePair(gPair);

    const esp_err_t bootRc = esp_ota_set_boot_partition(gTarget);
    if (bootRc != ESP_OK) {
        error = String("esp_ota_set_boot_") + esp_err_to_name(bootRc);
        gTarget = nullptr;
        gRunning = nullptr;
        return false;
    }

    gTarget = nullptr;
    gRunning = nullptr;
    gExpected = 0;
    gWritten = 0;
    return true;
}

void abort() {
    if (gHandle) esp_ota_abort(gHandle);
    gHandle = 0;
    gTarget = nullptr;
    gRunning = nullptr;
    gExpected = 0;
    gWritten = 0;
}

bool active() { return gHandle != 0; }

}  // namespace maz::safeota
