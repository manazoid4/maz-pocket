// Generated from protocol/action_ids.json by scripts/gen_action_ids.py.
// Do not edit by hand.
#pragma once

#include <stddef.h>

namespace maz {
namespace action_ids {

constexpr const char* PC_DESKTOP = "desktop";
constexpr const char* PC_PLAY_PAUSE = "play_pause";
constexpr const char* PC_MUTE = "mute";
constexpr const char* PC_VOLUME_DOWN = "volume_down";
constexpr const char* PC_VOLUME_UP = "volume_up";
constexpr const char* PC_PREVIOUS_TRACK = "previous_track";
constexpr const char* PC_NEXT_TRACK = "next_track";
constexpr const char* PC_LOCK = "lock";

constexpr const char* PC[] = {
    PC_DESKTOP, PC_PLAY_PAUSE, PC_MUTE, PC_VOLUME_DOWN,
    PC_VOLUME_UP, PC_PREVIOUS_TRACK, PC_NEXT_TRACK, PC_LOCK,
};
constexpr size_t PC_COUNT = sizeof(PC) / sizeof(PC[0]);

constexpr const char* CORE_GIT_STATUS = "git_status";
constexpr const char* CORE_GIT_FETCH = "git_fetch";
constexpr const char* CORE_GIT_PULL_FF = "git_pull_ff";
constexpr const char* CORE_TESTS = "tests";
constexpr const char* CORE_BUILD = "build";
constexpr const char* CORE_OPEN_FOLDER = "open_folder";

constexpr const char* CORE[] = {
    CORE_GIT_STATUS, CORE_GIT_FETCH, CORE_GIT_PULL_FF,
    CORE_TESTS, CORE_BUILD, CORE_OPEN_FOLDER,
};
constexpr size_t CORE_COUNT = sizeof(CORE) / sizeof(CORE[0]);

}  // namespace action_ids
}  // namespace maz
