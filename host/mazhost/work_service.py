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
        contributing_ids = {
            event_type["id"]
            for event_type in track["event_types"]
            if event_type["active"] and event_type["contributes_to_headline"]
        }
        return sum(
            event["value"]
            for event in events
            if event["track_id"] == track["id"]
            and event["event_type_id"] in contributing_ids
        )

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
        now = time.time()
        start, end = local_day_bounds(when=now)
        today_events = self.store.events_for_range(start=start, end=end)
        local_weekday = time.localtime(now).tm_wday
        week_start, _ = local_day_bounds(when=now, days_ago=local_weekday)
        week_events = self.store.events_for_range(start=week_start, end=end)
        tracks = [
            track
            for track in self.store.list_tracks(include_archived=False)
            if track["state"] == "active" and track["pinned"]
        ]
        tracks.sort(key=lambda track: (track["sort_order"], track["created_at"], track["id"]))

        cards = []
        for track in tracks:
            today_total = self._headline_count(track, today_events)
            current_window = "week" if track["cadence"] == "weekly" else "today"
            current_events = week_events if current_window == "week" else today_events
            current_total = self._headline_count(track, current_events)
            cards.append({
                "track_id": track["id"],
                "name": track["name"],
                "short_label": track["short_label"],
                "mode": track["mode"],
                "unit": track["unit"],
                "pinned": bool(track["pinned"]),
                "cadence": track["cadence"],
                "target": track["target"],
                "today_total": today_total,
                "current_total": current_total,
                "current_window": current_window,
                "breakdown": self._breakdown(track, current_events),
            })

        return {
            "ok": True,
            "window": window,
            "generated_at": now,
            "day_start": start,
            "day_end": end,
            "tracks": cards,
        }

    # -------------------------------------------------------------- history
    def history(self, *, days: int = 7) -> dict[str, Any]:
        days = max(1, min(days, 31))
        # Archived tracks disappear from today's quick-log surface but their
        # immutable historical attribution remains queryable.
        historical_tracks = self.store.list_tracks(include_archived=True)
        daily: list[dict[str, Any]] = []
        for offset in range(days - 1, -1, -1):
            start, end = local_day_bounds(days_ago=offset)
            events = self.store.events_for_range(start=start, end=end)
            per_track = {
                track["id"]: self._headline_count(track, events)
                for track in historical_tracks
            }
            daily.append({"day_start": start, "totals": per_track})
        return {"ok": True, "days": days, "history": daily}

    # ---------------------------------------------------------- cardputer
    def cardputer_payload(self) -> dict[str, Any]:
        """Hard-capped: 4 pinned tracks max, fixed 7-length day array."""
        summary = self.summary(window="today")
        pinned = summary["tracks"][:CARDPUTER_MAX_PINNED_TRACKS]
        history = self.history(days=CARDPUTER_HISTORY_DAYS)

        track_ids = [t["track_id"] for t in pinned]
        strip: list[dict[str, Any]] = []
        for day in history["history"]:
            strip.append({tid: day["totals"].get(tid, 0) for tid in track_ids})

        today = self.today()
        return {
            "ok": True,
            "generated_at": summary["generated_at"],
            "today_score": sum(t["current_total"] for t in pinned),
            "next_action": (
                today["recommendations"][0]["action"][:80]
                if today["recommendations"] else "Research one real prospect or vacancy"
            ),
            "tracks": [
                {
                    "track_id": t["track_id"],
                    "short_label": t["short_label"][:16],
                    # Firmware labels this compact field as the track's current
                    # value; for weekly cadence it is the current local week.
                    "today_total": t["current_total"],
                    "target": t["target"],
                    "primary_event_type_id": next(
                        (
                            event_type["event_type_id"]
                            for event_type in t["breakdown"]
                            if event_type["event_type_id"]
                            == self.store.get_track(t["track_id"])["primary_event_type_id"]
                        ),
                        "",
                    ),
                }
                for t in pinned
            ],
            "seven_day": strip[-CARDPUTER_HISTORY_DAYS:],
        }

    # ------------------------------------------------------------ pipelines
    def pipeline_summary(self) -> dict[str, Any]:
        now = time.time()
        clients = self.store.list_pipeline_items("client")
        jobs = self.store.list_pipeline_items("job")
        client_qualified = {"QUALIFIED", "READY_TO_SEND", "SENT", "FOLLOW_UP_DUE", "WON"}
        job_qualified = {"QUALIFIED", "READY_TO_APPLY", "APPLIED", "FOLLOW_UP_DUE", "INTERVIEW", "OFFER"}
        due = lambda item: item["due_at"] is not None and item["due_at"] <= now
        return {
            "clients": {
                "prospects_researched": len(clients),
                "qualified": sum(item["stage"] in client_qualified for item in clients),
                "outreach_ready": sum(item["stage"] == "READY_TO_SEND" for item in clients),
                "outreach_sent": sum(item["stage"] in {"SENT", "FOLLOW_UP_DUE", "WON", "LOST"} for item in clients),
                "follow_ups_due": sum(due(item) and item["stage"] in {"SENT", "FOLLOW_UP_DUE"} for item in clients),
                "mini_solutions": sum(item["proof_status"] == "READY" for item in clients),
            },
            "jobs": {
                "jobs_reviewed": len(jobs),
                "jobs_qualified": sum(item["stage"] in job_qualified for item in jobs),
                "applications_submitted": sum(item["stage"] in {"APPLIED", "FOLLOW_UP_DUE", "INTERVIEW", "OFFER", "REJECTED"} for item in jobs),
                "follow_ups_due": sum(due(item) and item["stage"] in {"APPLIED", "FOLLOW_UP_DUE"} for item in jobs),
                "interviews": sum(item["stage"] == "INTERVIEW" for item in jobs),
                "outcomes": sum(item["stage"] in {"OFFER", "REJECTED", "WITHDRAWN"} for item in jobs),
            },
        }

    def today(self) -> dict[str, Any]:
        now = time.time()
        items = self.store.list_pipeline_items()
        ranked: list[tuple[int, float, dict[str, Any]]] = []
        for item in items:
            action = item["next_action"].strip()
            if not action:
                continue
            due = item["due_at"] is not None and item["due_at"] <= now
            if due:
                priority = 0
                reason = "Stored follow-up or deadline is due now."
            elif item["pipeline"] == "client" and item["stage"] == "QUALIFIED" and item["proof_status"] != "READY":
                priority = 10
                reason = "Qualified prospect; proof is still needed before outreach."
            elif item["pipeline"] == "client" and item["stage"] == "READY_TO_SEND":
                priority = 20
                reason = "Personalised outreach and proof are ready, but nothing has been sent."
            elif item["pipeline"] == "job" and item["stage"] == "READY_TO_APPLY":
                priority = 30
                reason = "Qualified live vacancy is ready for the human application step."
            elif item["stage"] in {"REJECTED", "SKIPPED", "LOST", "WON", "OFFER", "WITHDRAWN"}:
                continue
            else:
                priority = 50
                reason = f"Stored {item['pipeline']} pipeline stage is {item['stage']}."
            ranked.append((priority, -(item["score"] or 0), {
                "item_id": item["item_id"], "pipeline": item["pipeline"],
                "stage": item["stage"], "action": action, "reason": reason,
                "source_url": item["source_url"],
            }))
        ranked.sort(key=lambda row: (row[0], row[1], row[2]["item_id"]))
        recommendations: list[dict[str, Any]] = []
        seen_categories: set[tuple[str, int]] = set()
        for priority, _score, item in ranked:
            category = (item["pipeline"], priority)
            if category in seen_categories:
                continue
            recommendations.append(item)
            seen_categories.add(category)
            if len(recommendations) == 3:
                break
        if len(recommendations) < 3:
            selected_ids = {item["item_id"] for item in recommendations}
            recommendations.extend(
                row[2]
                for row in ranked
                if row[2]["item_id"] not in selected_ids
            )
            recommendations = recommendations[:3]
        # Keep the daily answer useful across both active pipelines.  If a
        # qualified job exists but three client actions outrank it, reserve
        # the last slot for that independent job action rather than hiding it
        # behind a wall of outreach tasks.
        if not any(item["pipeline"] == "job" for item in recommendations):
            job_candidate = next((row[2] for row in ranked if row[2]["pipeline"] == "job"), None)
            if job_candidate:
                if len(recommendations) == 3:
                    recommendations[-1] = job_candidate
                else:
                    recommendations.append(job_candidate)
        work_summary = self.summary()
        work_progress = [
            {
                "track_id": track["track_id"],
                "today_total": track["today_total"],
                "current_total": track["current_total"],
                "target": track["target"],
            }
            for track in work_summary["tracks"]
        ]
        lines = ["TODAY"]
        if recommendations:
            for index, item in enumerate(recommendations, 1):
                lines.extend((f"{index}. {item['action']}", f"   Reason: {item['reason']}"))
        else:
            lines.extend(("1. Research one real prospect or vacancy.", "   Reason: no active pipeline action is stored."))
        return {
            "ok": True,
            "generated_at": now,
            "stats": {**self.pipeline_summary(), "work": work_progress},
            "recommendations": recommendations,
            "reply": "\n".join(lines),
        }
