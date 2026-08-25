#pragma once

#include <stdint.h>
#include <string>

namespace maz {
namespace field {

void begin();
void update();

void armContext(const std::string& appId, const std::string& snapshot);
const std::string& context();
bool contextArmed();
void clearContext();

std::string nowText();

const char* quickId(int slot);
const char* quickLabel(int slot);
bool runQuick(int slot);
void cycleQuick(int slot);

void toggleFieldMode();
void toggleShift();
std::string shiftElapsedText();

void requestSystemStatus();
void requestWorkSummary();
void queueBeam(const std::string& text);

}  // namespace field
}  // namespace maz
