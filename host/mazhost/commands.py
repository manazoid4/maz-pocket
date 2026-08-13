from __future__ import annotations

import re

REMINDER = re.compile(
    r"^remind me in (?P<amount>\d{1,4}) (?P<unit>minute|minutes|hour|hours) to (?P<title>.+)$",
    re.IGNORECASE,
)


def parse_command(text: str) -> dict | None:
    match = REMINDER.match(text.strip())
    if not match:
        return None
    amount = int(match.group("amount"))
    seconds = amount * (3600 if match.group("unit").lower().startswith("hour") else 60)
    if seconds > 30 * 86_400:
        return None
    return {
        "type": "reminder.create",
        "delay_seconds": seconds,
        "title": match.group("title").strip(),
    }
