"""WORK Consistency aggregation: summaries, 7-day history, Cardputer payload.

No derived composite metrics (close rate, productivity score) are computed
here, ever — that is a permanent v1 non-goal (spec Section 8)."""

from __future__ import annotations

import time
from typing import Any

from .work_store import WorkStore, local_day_bounds

CARDPUTER_MAX_PINNED_TRACKS = 4
CARDPUTER_HISTORY_DAYS = 7


class WorkService:
    def __init__(self, store: WorkStore) -> None:
        self.store = store

    # ---------------------------------------------------------------- utils
    def _headline_count(self, track: dict[str, Any], events: list[dict[str, Any]]) -> float:
        primary_id = track["primary_event_type_id"]
        return sum(e["value"] for e in events if e["track_id"] == track["id"] and e["event_type_id"] == primary_id)

    def _breakdown(self, track: dict[str, Any], events: list[dict[str, Any]]) -> list[dict[str, Any]]:
        totals: dict[str, float] = {}
        for e in events:
            if e["track_id"] != track["id"]:
                continue
            totals[e["event_type_id"]] = totals.get(e["event_type_id"], 0) + e["value"]
        result = []
        for et in track["event_types"]:
            result.append({
                "event_type_id": et["id"],
                "label": et["label"],
                "total": totals.get(et["id"], 0),
                "contributes_to_headline": bool(et["contributes_to_headline"]),
            })
        return result

    # -------------------------------------------------------------- summary
    def summary(self, *, window: str = "today") -> dict[str, Any]:
        start, end = local_day_bounds()
        events = self.store.events_for_range(start=start, end=end)
        tracks = [t for t in self.store.list_tracks(include_archived=False) if t["state"] == "active"]
        tracks.sort(key=lambda t: (0 if t["pinned"] else 1, t["sort_order"]))

        cards = []
        for track in tracks:
            headline = self._headline_count(track, events)
            cards.append({
                "track_id": track["id"],
                "name": track["name"],
                "short_label": track["short_label"],
                "mode": track["mode"],
                "unit": track["unit"],
                "pinned": bool(track["pinned"]),
                "target": track["target"],
                "today_total": headline,
                "breakdown": self._breakdown(track, events),
            })

        return {
            "ok": True,
            "window": window,
            "generated_at": time.time(),
            "day_start": start,
            "day_end": end,
            "tracks": cards,
        }

    # -------------------------------------------------------------- history
    def history(self, *, days: int = 7) -> dict[str, Any]:
        days = max(1, min(days, 31))
        active_tracks = [t for t in self.store.list_tracks(include_archived=False) if t["state"] != "archived"]
        daily: list[dict[str, Any]] = []
        for offset in range(days - 1, -1, -1):
            start, end = local_day_bounds(days_ago=offset)
            events = self.store.events_for_range(start=start, end=end)
            per_track = {
                t["id"]: self._headline_count(t, events) for t in active_tracks
            }
            daily.append({"day_start": start, "totals": per_track})
        return {"ok": True, "days": days, "history": daily}

    # ---------------------------------------------------------- cardputer
    def cardputer_payload(self) -> dict[str, Any]:
        """Hard-capped: 4 pinned tracks max, fixed 7-length day array."""
        summary = self.summary(window="today")
        pinned = [t for t in summary["tracks"] if t["pinned"]][:CARDPUTER_MAX_PINNED_TRACKS]
        history = self.history(days=CARDPUTER_HISTORY_DAYS)

        track_ids = [t["track_id"] for t in pinned]
        strip: list[dict[str, Any]] = []
        for day in history["history"]:
            strip.append({tid: day["totals"].get(tid, 0) for tid in track_ids})

        return {
            "ok": True,
            "generated_at": summary["generated_at"],
            "tracks": [
                {
                    "track_id": t["track_id"],
                    "short_label": t["short_label"][:16],
                    "today_total": t["today_total"],
                    "target": t["target"],
                }
                for t in pinned
            ],
            "seven_day": strip[-CARDPUTER_HISTORY_DAYS:],
        }
