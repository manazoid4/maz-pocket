from __future__ import annotations

import re

REMINDER = re.compile(
    r"^remind me in (?P<amount>\d{1,4}) (?P<unit>minute|minutes|hour|hours) to (?P<title>.+)$",
    re.IGNORECASE,
)

# Keep these deliberately narrow. A phrase must look like a direct command,
# not merely mention the idea, before MAZ Host touches the computer.
PC_PATTERNS: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"^(?:please )?(?:show (?:me )?)?(?:the )?desktop$", re.I), "desktop"),
    (re.compile(r"^(?:please )?(?:play|pause|play pause|pause music|resume music|toggle media)$", re.I), "play_pause"),
    (re.compile(r"^(?:please )?(?:mute|mute (?:my|the) (?:pc|computer)|toggle mute)$", re.I), "mute"),
    (re.compile(r"^(?:please )?(?:volume down|turn (?:the )?volume down|lower (?:the )?volume)$", re.I), "volume_down"),
    (re.compile(r"^(?:please )?(?:volume up|turn (?:the )?volume up|raise (?:the )?volume)$", re.I), "volume_up"),
    (re.compile(r"^(?:please )?(?:previous track|go back a track|previous song)$", re.I), "previous_track"),
    (re.compile(r"^(?:please )?(?:next track|skip track|next song)$", re.I), "next_track"),
    (re.compile(r"^(?:please )?(?:lock (?:my|the) (?:pc|computer)|lock pc|lock computer)$", re.I), "lock"),
]


def parse_command(text: str) -> dict | None:
    stripped = text.strip()
    match = REMINDER.match(stripped)
    if match:
        amount = int(match.group("amount"))
        seconds = amount * (3600 if match.group("unit").lower().startswith("hour") else 60)
        if seconds <= 30 * 86_400:
            return {
                "type": "reminder.create",
                "delay_seconds": seconds,
                "title": match.group("title").strip(),
            }

    for pattern, action in PC_PATTERNS:
        if pattern.match(stripped):
            return {"type": "pc.action", "action": action}
    return None
