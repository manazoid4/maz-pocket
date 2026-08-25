"""WORK Consistency API routes.

Mounted onto the same authenticated phone-control app as AUTHORITY (spec
Section 4 lists these paths as `/work/...`; they are reachable here as
`/control/work/...` because the session cookie set by phone_control.py is
scoped to path=/control — mounting elsewhere would silently drop the cookie).
Same session boundary, no new auth mechanism."""

from __future__ import annotations

import time
import uuid
from typing import Annotated, Literal

from fastapi import Cookie, FastAPI, HTTPException, Request
from pydantic import BaseModel, Field

from .work_service import WorkService
from .work_store import WorkStore, WorkStoreError


class CreateTrackBody(BaseModel):
    name: str = Field(min_length=1, max_length=80)
    short_label: str = Field(min_length=1, max_length=20)
    mode: Literal["count", "time", "checkin"]
    unit: str = Field(min_length=1, max_length=30)
    cadence: Literal["daily", "weekly", "none"] = "daily"
    target: float | None = Field(default=None, ge=0)
    event_types: list[str] = Field(min_length=1, max_length=12)
    primary_event_type_index: int = Field(default=0, ge=0)
    pinned: bool = False
    sort_order: int = Field(default=0, ge=0, le=10_000)


class UpdateTrackBody(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=80)
    short_label: str | None = Field(default=None, min_length=1, max_length=20)
    target: float | None = Field(default=None, ge=0)
    cadence: Literal["daily", "weekly", "none"] | None = None
    pinned: bool | None = None
    sort_order: int | None = Field(default=None, ge=0, le=10_000)
    state: Literal["active", "paused", "archived"] | None = None


class CreateEventBody(BaseModel):
    event_id: str | None = Field(default=None, max_length=80)
    event_type_id: str = Field(min_length=1, max_length=120)
    value: float = Field(default=1, gt=0, le=100_000)
    occurred_at: float | None = None
    source: Literal["phone_manual", "cardputer_manual", "import"] = "phone_manual"
    note: str = Field(default="", max_length=500)


def install_work_routes(
    app: FastAPI,
    *,
    service: WorkService,
    store: WorkStore,
    require_session,
    session_identity,
) -> None:
    @app.get("/work/summary")
    def work_summary(
        request: Request,
        window: str = "today",
        maz_control_session: Annotated[str | None, Cookie()] = None,
    ):
        require_session(request, maz_control_session)
        return service.summary(window=window)

    @app.get("/work/tracks")
    def work_tracks(request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        return {"ok": True, "tracks": store.list_tracks(include_archived=True)}

    @app.post("/work/tracks")
    def work_create_track(
        body: CreateTrackBody,
        request: Request,
        maz_control_session: Annotated[str | None, Cookie()] = None,
    ):
        require_session(request, maz_control_session)
        if body.primary_event_type_index >= len(body.event_types):
            raise HTTPException(422, "primary_event_type_index_out_of_range")
        track_id = "custom_" + uuid.uuid4().hex[:12]
        event_types = [
            (f"{track_id}.{i}", label.strip()[:60], i == body.primary_event_type_index)
            for i, label in enumerate(body.event_types)
            if label.strip()
        ]
        if not event_types:
            raise HTTPException(422, "at_least_one_event_type_required")
        try:
            track = store.create_track(
                track_id=track_id,
                name=body.name.strip(),
                short_label=body.short_label.strip(),
                mode=body.mode,
                unit=body.unit.strip(),
                cadence=body.cadence,
                target=body.target,
                event_types=event_types,
                primary_event_type_id=event_types[body.primary_event_type_index][0],
                pinned=body.pinned,
                sort_order=body.sort_order,
            )
        except WorkStoreError as error:
            raise HTTPException(400, str(error)) from error
        return {"ok": True, "track": track}

    @app.patch("/work/tracks/{track_id}")
    def work_update_track(
        track_id: str,
        body: UpdateTrackBody,
        request: Request,
        maz_control_session: Annotated[str | None, Cookie()] = None,
    ):
        require_session(request, maz_control_session)
        fields = {k: v for k, v in body.model_dump().items() if v is not None}
        try:
            track = store.update_track(track_id, fields)
        except WorkStoreError as error:
            raise HTTPException(404, str(error)) from error
        return {"ok": True, "track": track}

    @app.post("/work/tracks/{track_id}/events")
    def work_create_event(
        track_id: str,
        body: CreateEventBody,
        request: Request,
        maz_control_session: Annotated[str | None, Cookie()] = None,
    ):
        session_id = session_identity(request, maz_control_session)
        event_id = body.event_id or ("evt_" + uuid.uuid4().hex)
        try:
            event, created = store.create_event(
                event_id=event_id,
                track_id=track_id,
                event_type_id=body.event_type_id,
                value=body.value,
                occurred_at=body.occurred_at if body.occurred_at is not None else time.time(),
                source=body.source,
                note=body.note,
                session_id=session_id,
            )
        except WorkStoreError as error:
            raise HTTPException(400, str(error)) from error
        return {"ok": True, "event": event, "created": created}

    @app.post("/work/events/{event_id}/undo")
    def work_undo(
        event_id: str,
        request: Request,
        maz_control_session: Annotated[str | None, Cookie()] = None,
    ):
        session_id = session_identity(request, maz_control_session)
        try:
            reversal = store.undo_last(session_id=session_id, expected_event_id=event_id)
        except WorkStoreError as error:
            raise HTTPException(400, str(error)) from error
        return {"ok": True, "reversal": reversal}

    @app.get("/work/history")
    def work_history(
        request: Request,
        days: int = 7,
        maz_control_session: Annotated[str | None, Cookie()] = None,
    ):
        require_session(request, maz_control_session)
        return service.history(days=days)
