// MAZ Pocket — the control surface, over USB and over Wi-Fi.
//
// The MAZ* command language used to live inline in main.cpp and answer only on
// the serial port, which meant the device could neither be driven nor updated
// without a cable. It lives here now so both transports run exactly one parser
// and cannot drift apart.
//
// Wi-Fi control is deliberately gated. A client on the LAN that can send
// MAZKEY can type into your notes, so a connection is refused until it
// presents the same token the device already uses to reach MAZ Host, and OTA
// carries that token as its password.
#pragma once

#include <Arduino.h>

namespace maz {
namespace control {

// Starts the TCP listener and the OTA responder. Safe to call before Wi-Fi is
// up: both are armed by update() once a connection exists.
void begin();

// Pump. Services OTA, accepts one client at a time, and reads whole lines from
// the serial port.
void update();

// Runs one command and returns the reply line. `trusted` is true for USB,
// where physical access is already total, and for a network client that has
// authenticated.
String handleLine(const String& line, bool trusted);

// True once the listener is bound, so Connections can show where to reach it.
bool listening();

}  // namespace control
}  // namespace maz
