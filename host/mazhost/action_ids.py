"""Generated from protocol/action_ids.json by scripts/gen_action_ids.py."""

PC_ACTIONS = (
    "desktop",
    "play_pause",
    "mute",
    "volume_down",
    "volume_up",
    "previous_track",
    "next_track",
    "lock",
)

CORE_ACTIONS = (
    "git_status",
    "git_fetch",
    "git_pull_ff",
    "tests",
    "build",
    "open_folder",
)

PC_ACTION_SET = frozenset(PC_ACTIONS)
CORE_ACTION_SET = frozenset(CORE_ACTIONS)
