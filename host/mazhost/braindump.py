from __future__ import annotations

import json
import re


def _fallback(transcript: str) -> dict:
    cleaned = " ".join(transcript.strip().split())
    if not cleaned:
        return {
            "summary": "No clear speech was transcribed.",
            "ideas": [],
            "actions": [],
            "questions": [],
        }

    sentences = [
        sentence.strip()
        for sentence in re.split(r"(?<=[.!?])\s+", cleaned)
        if sentence.strip()
    ]
    actions: list[str] = []
    ideas: list[str] = []
    questions = [sentence for sentence in sentences if sentence.endswith("?")]

    for sentence in sentences:
        lower = sentence.lower()
        for marker in ("the action is to ", "action is to ", "i need to ", "i will "):
            if marker in lower:
                start = lower.index(marker) + len(marker)
                action = sentence[start:].strip(" .")
                if action and action not in actions:
                    actions.append(action)
                break
        if any(marker in lower for marker in ("the idea is ", "idea is ", "maybe ", "could ")):
            ideas.append(sentence.strip(" ."))

    return {
        "summary": cleaned[:280],
        "ideas": ideas[:5],
        "actions": actions[:5],
        "questions": questions[:5],
    }


def structure_braindump(reply: str, transcript: str) -> tuple[dict, bool]:
    """Return Cardputer-safe structure and whether deterministic fallback ran."""
    candidate = reply.strip()
    if candidate.startswith("```"):
        lines = candidate.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].strip() == "```":
            lines = lines[:-1]
        candidate = "\n".join(lines).strip()

    try:
        value = json.loads(candidate)
    except (json.JSONDecodeError, TypeError):
        return _fallback(transcript), True

    if not isinstance(value, dict) or not isinstance(value.get("summary"), str):
        return _fallback(transcript), True

    for key in ("ideas", "actions", "questions"):
        if not isinstance(value.get(key), list):
            value[key] = []
        value[key] = [str(item) for item in value[key]][:5]
    value["summary"] = value["summary"][:280]
    return value, False
