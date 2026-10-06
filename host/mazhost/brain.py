"""Free context + guards that make nod answer instead of refusing."""

from __future__ import annotations

import ast
import operator
import re
import time
from pathlib import Path

import httpx

REFUSAL = re.compile(
    r"(i (do not|don't|do n't) have (access|the ability|real-?time|a way)|"
    r"as an ai\b|as a language model|i('m| am) (not able|unable) to|"
    r"i (can't|cannot|can not) (access|provide|check|browse|tell|know|help with)|"
    r"i (don't|do not) (know|have) (the )?(current|today|real)|"
    r"(unable|not able) to (access|browse|check)|i have no (way|access))",
    re.I,
)


def is_refusal(text: str) -> bool:
    return bool(REFUSAL.search(text or ""))


_OPS = {
    ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
    ast.Div: operator.truediv, ast.Pow: operator.pow, ast.USub: operator.neg,
    ast.Mod: operator.mod,
}


def _eval(node):
    if isinstance(node, ast.Expression):
        return _eval(node.body)
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return node.value
    if isinstance(node, ast.BinOp) and type(node.op) in _OPS:
        left, right = _eval(node.left), _eval(node.right)
        if isinstance(node.op, ast.Pow) and abs(right) > 10:
            raise ValueError("exponent too large")
        return _OPS[type(node.op)](left, right)
    if isinstance(node, ast.UnaryOp) and type(node.op) in _OPS:
        return _OPS[type(node.op)](_eval(node.operand))
    raise ValueError("unsupported")


def maths_line(text: str) -> str:
    """Solve 'N% of M' and plain arithmetic exactly so the model never guesses."""
    t = text.lower().replace(",", "").replace("?", " ")
    t = re.sub(r"(\d+(?:\.\d+)?)\s*(?:%|percent|per cent)\s*of\s*(\d+(?:\.\d+)?)", r"(\1/100*\2)", t)
    t = t.replace("x", "*").replace("×", "*").replace("÷", "/")
    t = re.sub(r"\b(plus)\b", "+", t)
    t = re.sub(r"\b(minus)\b", "-", t)
    t = re.sub(r"\b(times|multiplied by)\b", "*", t)
    t = re.sub(r"\b(divided by|over)\b", "/", t)
    m = re.search(r"[\d(][\d\s.+\-*/()%^]*[\d)]", t)
    if not m or not re.search(r"[+\-*/]", m.group(0)):
        return ""
    try:
        value = _eval(ast.parse(m.group(0).replace("^", "**").strip(), mode="eval"))
    except (ValueError, SyntaxError, ZeroDivisionError, TypeError, OverflowError):
        return ""
    return f"\nExact maths result for the user's question: {round(value, 4):g}. State it directly.\n"


_WMO = {0: "clear", 1: "mostly clear", 2: "partly cloudy", 3: "overcast", 45: "foggy", 48: "foggy",
        51: "light drizzle", 53: "drizzle", 55: "heavy drizzle", 61: "light rain", 63: "rain",
        65: "heavy rain", 71: "light snow", 73: "snow", 75: "heavy snow", 80: "showers",
        81: "showers", 82: "heavy showers", 95: "thunderstorms", 96: "thunderstorms", 99: "thunderstorms"}
_weather_cache: dict[str, tuple[float, str]] = {}
# London default; ponytail: single fixed location, add geocoding when the owner travels.
LAT, LON = 51.5072, -0.1276


def weather_line(text: str) -> str:
    if not re.search(r"weather|rain|umbrella|temperature|cold|warm|hot outside|forecast|sunny|snow|wind", text, re.I):
        return ""
    hit = _weather_cache.get("w")
    if hit and time.time() - hit[0] < 600:
        return hit[1]
    try:
        r = httpx.get(
            "https://api.open-meteo.com/v1/forecast",
            params={"latitude": LAT, "longitude": LON, "timezone": "Europe/London",
                    "current": "temperature_2m,apparent_temperature,weather_code,wind_speed_10m",
                    "daily": "temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code",
                    "forecast_days": 1},
            timeout=3.0,
        )
        r.raise_for_status()
        j = r.json()
        c, d = j["current"], j["daily"]
        line = (
            f"\nLive weather, London UK: now {c['temperature_2m']:.0f}C (feels {c['apparent_temperature']:.0f}C), "
            f"{_WMO.get(c['weather_code'], 'mixed')}, wind {c['wind_speed_10m']:.0f} km/h. "
            f"Today high {d['temperature_2m_max'][0]:.0f}C, low {d['temperature_2m_min'][0]:.0f}C, "
            f"{_WMO.get(d['weather_code'][0], 'mixed')}, rain chance {d['precipitation_probability_max'][0]}%. "
            "Use this to answer weather questions.\n"
        )
    except Exception:  # network/parse: just skip the context, never fail the turn
        return ""
    _weather_cache["w"] = (time.time(), line)
    return line


def priorities_line(text: str, now_path: str) -> str:
    if not now_path or not re.search(
        r"work on|focus|priorit|what next|what now|should i do|my projects|what am i|working on|and after|after that|next", text, re.I
    ):
        return ""
    try:
        raw = Path(now_path).expanduser().read_text(encoding="utf-8")
    except OSError:
        return ""
    keep = [ln.rstrip() for ln in raw.splitlines()
            if ln.startswith("## ") or ln.strip().startswith("- [ ]")]
    body = "\n".join(keep)
    body = body.split("## Later")[0][:1500]
    return ("\nOwner's current priorities (from NOW.md, in order; the first unchecked item of "
            "project 1 is the best next action):\n" + body + "\n")
