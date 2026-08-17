from __future__ import annotations

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from .teach_capture import TeachCapture, TeachError


class TeachStartRequest(BaseModel):
    display_id: str = Field(default="primary", max_length=120)
    region: dict[str, int] | None = None
    fps: int = Field(default=15, ge=2, le=30)
    audio_device: str = Field(default="", max_length=240)
    include_cursor: bool = True


class TeachMarkRequest(BaseModel):
    note: str = Field(default="", max_length=240)


class TeachStopRequest(BaseModel):
    transcribe: bool = True


def install_teach_routes(api: FastAPI, service: TeachCapture) -> None:
    @api.get("/teach/status")
    def teach_capabilities():
        return {"ok": True, "available": service.available(), "displays": service.displays()}

    @api.post("/teach/start")
    def teach_start(body: TeachStartRequest):
        try:
            return {"ok": True, "session": service.start(
                display_id=body.display_id,
                region=body.region,
                fps=body.fps,
                audio_device=body.audio_device,
                include_cursor=body.include_cursor,
            )}
        except TeachError as error:
            raise HTTPException(400, str(error)) from error

    @api.post("/teach/{session_id}/mark")
    def teach_mark(session_id: str, body: TeachMarkRequest):
        try:
            return {"ok": True, "mark": service.mark(session_id, body.note)}
        except TeachError as error:
            raise HTTPException(400, str(error)) from error

    @api.post("/teach/{session_id}/stop")
    def teach_stop(session_id: str, body: TeachStopRequest):
        try:
            return {"ok": True, "session": service.stop(session_id, transcribe=body.transcribe)}
        except TeachError as error:
            raise HTTPException(400, str(error)) from error

    @api.get("/teach/{session_id}")
    def teach_status(session_id: str):
        try:
            return {"ok": True, "session": service.status(session_id)}
        except TeachError as error:
            raise HTTPException(404, str(error)) from error

    @api.get("/teach")
    def teach_recent(limit: int = 20):
        return {"ok": True, "sessions": service.recent(limit)}
