#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace maz::safeota {

struct Status {
    bool     safe = false;
    String   reason;
    uint32_t runningAddress = 0;
    uint32_t targetAddress = 0;
    uint32_t targetSize = 0;
};

Status status(size_t firmwareSize = 0);

bool begin(size_t firmwareSize, String& error);
bool write(const uint8_t* data, size_t size, String& error);
bool finish(String& error);
void abort();
bool active();

}  // namespace maz::safeota
