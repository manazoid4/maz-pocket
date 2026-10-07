"""Needs-you queue API. Mounted on the main app, so the global bearer-token dependency applies."""

from __future__ import annotations

from fastapi import APIRouter, HTTPException, Query

from .buddy import Buddy
from .needs import Needs
from .voices import resolve_data_dir


def build_needs_router(cfg, buddy: Buddy) -> APIRouter:
    router = APIRouter(prefix="/needs", tags=["needs"])
    needs = Needs(buddy, resolve_data_dir(cfg))

    def check(item_id: str) -> None:
        if item_id.startswith("a-"):
            raise HTTPException(400, "approvals_decide_on_device")
        if not item_id.startswith(("pr-", "act-")):
            raise HTTPException(404, "need_not_found")

    @router.get("")
    def get_needs():
        return needs.queue()

    @router.post("/{item_id}/done")
    def done(item_id: str):
        check(item_id)
        needs.mark(item_id, done=True)
        return {"ok": True, "id": item_id}

    @router.post("/{item_id}/snooze")
    def snooze(item_id: str, hours: float = Query(default=24, gt=0, le=24 * 30)):
        check(item_id)
        needs.mark(item_id, hours=hours)
        return {"ok": True, "id": item_id, "hours": hours}

    return router
