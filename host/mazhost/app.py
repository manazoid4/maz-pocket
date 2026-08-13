from __future__ import annotations

import json
import tempfile
import time
from pathlib import Path
from typing import Annotated, Literal

import httpx
from fastapi import Depends, FastAPI, File, Form, Header, HTTPException, Request, UploadFile
from pydantic import BaseModel, Field

from .config import Settings
from .commands import parse_command
from .device import DeviceMonitor
from .llm import Models, Route
from .nudge import NudgeClient
from .prompts import EXTRACT_PROMPTS, SYSTEM_PROMPT
from .refine import refine
from .security import Security
from .sessions import SessionStore
from .stt import SpeechToText


class TextTurn(BaseModel):
    text: str = Field(min_length=1, max_length=8_000)
    session_id: str
    route: Route = "auto"


class ExtractRequest(BaseModel):
    kind: Literal["decision", "debrief", "inbox"]
    text: str = Field(min_length=1, max_length=20_000)


def create_app(
    settings: Settings | None = None,
    *,
    stt: SpeechToText | None = None,
    models: Models | None = None,
    nudge: NudgeClient | None = None,
    device: DeviceMonitor | None = None,
) -> FastAPI:
    cfg = settings or Settings()
    security = Security(cfg)
    speech = stt or SpeechToText(cfg)
    model_router = models or Models(cfg)
    nudge_client = nudge or NudgeClient(cfg)
    device_monitor = device or DeviceMonitor(cfg)
    sessions = SessionStore(cfg.max_turns, cfg.session_ttl_minutes)
    api = FastAPI(
        title="MAZ Host",
        version="0.2.0",
        dependencies=[Depends(security.authorize)],
    )

    def grounded_messages(session_id: str, text: str) -> list[dict[str, str]]:
        history = sessions.messages(session_id)
        if not history and not sessions.has(session_id):
            raise HTTPException(404, "session_not_found")
        context = ""
        if any(word in text.lower() for word in ("agent", "sync", "nudge", "stale", "working")):
            try:
                context = "\nAgent Nudge evidence:\n" + json.dumps(nudge_client.summary())
            except (RuntimeError, httpx.HTTPError):
                context = "\nAgent Nudge is unavailable; say that plainly."
        return [{"role": "system", "content": SYSTEM_PROMPT + context}, *history, {"role": "user", "content": text}]

    def answer(session_id: str, text: str, route: Route) -> dict:
        refined = refine(text)
        command = parse_command(refined.text)
        if command:
            reply = f"Reminder set: {command['title']}"
            sessions.add_turn(session_id, refined.text, reply)
            return {
                "text": refined.text,
                "reply": reply,
                "provider": "deterministic-local",
                "actions": refined.actions,
                "commands": [command],
                "timings": {"llm_ms": 0},
            }
        started = time.perf_counter()
        try:
            reply, provider = model_router.chat(grounded_messages(session_id, refined.text), route)
        except RuntimeError as error:
            raise HTTPException(503, str(error)) from error
        llm_ms = round((time.perf_counter() - started) * 1000)
        sessions.add_turn(session_id, refined.text, reply)
        return {
            "text": refined.text,
            "reply": reply,
            "provider": provider,
            "actions": refined.actions,
            "timings": {"llm_ms": llm_ms},
        }

    @api.get("/health")
    def health():
        return {
            "ok": True,
            "stt": speech.available(),
            "llm": model_router.status(),
            "nudge": nudge_client.status(),
            "tts": cfg.tts_enabled,
        }

    @api.get("/models")
    def models_status():
        return {**model_router.status(), "default_route": cfg.default_route}

    @api.get("/device")
    def device_status():
        return device_monitor.status()

    @api.get("/device/logs")
    def device_logs(limit: int = 100):
        return {"lines": device_monitor.logs(max(1, min(limit, 500)))}

    @api.post("/device/monitor/start")
    def device_monitor_start():
        return device_monitor.start()

    @api.post("/device/monitor/stop")
    def device_monitor_stop():
        return device_monitor.stop()

    @api.post("/session/start")
    def session_start():
        return {"session_id": sessions.start()}

    @api.post("/session/end")
    def session_end(session_id: str):
        return {"ended": sessions.end(session_id)}

    @api.post("/turn/text")
    def turn_text(turn: TextTurn):
        return answer(turn.session_id, turn.text, turn.route)

    @api.post("/turn")
    async def turn_audio(
        audio: Annotated[UploadFile, File()],
        session_id: Annotated[str, Form()],
        route: Annotated[Route, Form()] = "auto",
    ):
        upload_started = time.perf_counter()
        suffix = Path(audio.filename or "audio.wav").suffix or ".wav"
        with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as target:
            size = 0
            while chunk := await audio.read(64 * 1024):
                size += len(chunk)
                if size > cfg.max_upload_mb * 1024 * 1024:
                    Path(target.name).unlink(missing_ok=True)
                    raise HTTPException(413, "audio_too_large")
                target.write(chunk)
            path = Path(target.name)
        try:
            security.validate_upload(path, size)
            upload_ms = round((time.perf_counter() - upload_started) * 1000)
            stt_started = time.perf_counter()
            text = speech.transcribe(path)
            stt_ms = round((time.perf_counter() - stt_started) * 1000)
            result = answer(session_id, text, route)
            result["timings"].update({"upload_ms": upload_ms, "stt_ms": stt_ms})
            return result
        finally:
            path.unlink(missing_ok=True)

    @api.post("/turn/raw")
    async def turn_audio_raw(
        request: Request,
        x_maz_session: Annotated[str, Header()],
        x_maz_route: Annotated[Route, Header()] = "auto",
    ):
        """ESP32-friendly WAV upload; the body is streamed, never buffered."""
        upload_started = time.perf_counter()
        with tempfile.NamedTemporaryFile(delete=False, suffix=".wav") as target:
            size = 0
            async for chunk in request.stream():
                size += len(chunk)
                if size > cfg.max_upload_mb * 1024 * 1024:
                    Path(target.name).unlink(missing_ok=True)
                    raise HTTPException(413, "audio_too_large")
                target.write(chunk)
            path = Path(target.name)
        try:
            security.validate_upload(path, size)
            upload_ms = round((time.perf_counter() - upload_started) * 1000)
            stt_started = time.perf_counter()
            text = speech.transcribe(path)
            stt_ms = round((time.perf_counter() - stt_started) * 1000)
            result = answer(x_maz_session, text, x_maz_route)
            result["timings"].update({"upload_ms": upload_ms, "stt_ms": stt_ms})
            return result
        finally:
            path.unlink(missing_ok=True)

    @api.post("/extract")
    def extract(body: ExtractRequest):
        prompt = EXTRACT_PROMPTS[body.kind]
        try:
            reply, provider = model_router.chat(
                [{"role": "system", "content": prompt + " Return JSON only."}, {"role": "user", "content": body.text}],
                cfg.default_route,
            )
            return {"kind": body.kind, "provider": provider, "result": json.loads(reply)}
        except (RuntimeError, json.JSONDecodeError) as error:
            raise HTTPException(503, f"extraction_failed: {error}") from error

    @api.post("/braindump")
    async def braindump(
        audio: Annotated[UploadFile, File()],
        highlights: Annotated[str, Form()] = "[]",
    ):
        with tempfile.NamedTemporaryFile(delete=False, suffix=".wav") as target:
            data = await audio.read(cfg.max_upload_mb * 1024 * 1024 + 1)
            target.write(data)
            path = Path(target.name)
        try:
            security.validate_upload(path, len(data))
            transcript = speech.transcribe(path)
            marks = json.loads(highlights)
            prompt = EXTRACT_PROMPTS["braindump"] + f" Highlights: {marks}. Return JSON only."
            reply, provider = model_router.chat(
                [{"role": "system", "content": prompt}, {"role": "user", "content": transcript}],
                cfg.default_route,
            )
            return {"transcript": transcript, "provider": provider, **json.loads(reply), "highlights": marks}
        except (json.JSONDecodeError, RuntimeError) as error:
            raise HTTPException(503, f"processing_failed: {error}") from error
        finally:
            path.unlink(missing_ok=True)

    @api.post("/braindump/raw")
    async def braindump_raw(
        request: Request,
        x_maz_highlights: Annotated[str, Header()] = "[]",
    ):
        """Streaming BrainDump upload with highlight seconds in one header."""
        with tempfile.NamedTemporaryFile(delete=False, suffix=".wav") as target:
            size = 0
            async for chunk in request.stream():
                size += len(chunk)
                if size > cfg.max_upload_mb * 1024 * 1024:
                    Path(target.name).unlink(missing_ok=True)
                    raise HTTPException(413, "audio_too_large")
                target.write(chunk)
            path = Path(target.name)
        try:
            security.validate_upload(path, size)
            transcript = speech.transcribe(path)
            marks = json.loads(x_maz_highlights)
            prompt = EXTRACT_PROMPTS["braindump"] + f" Highlights: {marks}. Return JSON only."
            reply, provider = model_router.chat(
                [{"role": "system", "content": prompt}, {"role": "user", "content": transcript}],
                cfg.default_route,
            )
            return {"transcript": transcript, "provider": provider, **json.loads(reply), "highlights": marks}
        except (json.JSONDecodeError, RuntimeError) as error:
            raise HTTPException(503, f"processing_failed: {error}") from error
        finally:
            path.unlink(missing_ok=True)

    @api.get("/nudge")
    def nudge_summary():
        try:
            return nudge_client.summary()
        except (RuntimeError, httpx.HTTPError) as error:
            raise HTTPException(503, str(error)) from error

    @api.get("/nudge/{session_id}")
    def nudge_detail(session_id: str):
        try:
            return nudge_client.detail(session_id)
        except (RuntimeError, httpx.HTTPError) as error:
            raise HTTPException(503, str(error)) from error

    @api.post("/nudge/{session_id}/nudge")
    def send_nudge(session_id: str):
        try:
            return nudge_client.nudge(session_id)
        except (RuntimeError, httpx.HTTPError) as error:
            raise HTTPException(503, str(error)) from error

    return api


app = create_app()
