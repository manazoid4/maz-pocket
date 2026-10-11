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
void toggleHalt();  // nod Halt (Fn+H): ask Core to deny every agent approval, or resume
std::string shiftElapsedText();

void requestSystemStatus();
void requestWorkSummary();
bool requestWorkIncrement(const std::string& trackId, const std::string& eventTypeId);
void queueBeam(const std::string& text);

}  // namespace field
}  // namespace maz
