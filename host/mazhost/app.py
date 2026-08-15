from __future__ import annotations

import json
import tempfile
import time
from pathlib import Path
from typing import Annotated, Literal

import httpx
from fastapi import Depends, FastAPI, File, Form, Header, HTTPException, Request, UploadFile
from fastapi.background import BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, Response
from pydantic import BaseModel, Field

from .braindump import structure_braindump
from .bridge import BridgeWorker
from .commands import parse_command
from .config import Settings
from .core import CoreError, MazCore
from .device import DeviceMonitor
from .jobs import CoreJobs
from .llm import Models, Route
from .nudge import NudgeClient
from .pc import PCController
from .prompts import EXTRACT_PROMPTS, SYSTEM_PROMPT
from .refine import refine
from .security import Security
from .sessions import SessionStore
from .stt import SpeechToText
from .tts import SpeechOut
from .versioning import build_id, core_version


class TextTurn(BaseModel):
    text: str = Field(min_length=1, max_length=8_000)
    session_id: str
    route: Route = "local"


class ExtractRequest(BaseModel):
    kind: Literal["decision", "debrief", "inbox"]
    text: str = Field(min_length=1, max_length=20_000)


class SpeakRequest(BaseModel):
    text: str = Field(min_length=1, max_length=1_400)


class PCActionRequest(BaseModel):
    action: Literal[
        "desktop",
        "play_pause",
        "mute",
        "volume_down",
        "volume_up",
        "previous_track",
        "next_track",
        "lock",
    ]


class CoreActionRequest(BaseModel):
    action: Literal["git_status", "git_fetch", "git_pull_ff", "tests", "build", "open_folder"]
    project: str = Field(min_length=1, max_length=160)


def create_app(
    settings: Settings | None = None,
    *,
    stt: SpeechToText | None = None,
    models: Models | None = None,
    nudge: NudgeClient | None = None,
    device: DeviceMonitor | None = None,
    pc: PCController | None = None,
    core: MazCore | None = None,
    bridge: BridgeWorker | None = None,
) -> FastAPI:
    cfg = settings or Settings()
    security = Security(cfg)
    speech = stt or SpeechToText(cfg)
    speech_out = SpeechOut(cfg)
    model_router = models or Models(cfg)
    nudge_client = nudge or NudgeClient(cfg)
    device_monitor = device or DeviceMonitor(cfg)
    pc_controller = pc or PCController()
    core_service = core or MazCore(cfg)
    core_jobs = CoreJobs(core_service)
    bridge_worker = bridge or BridgeWorker(cfg, core_service)
    sessions = SessionStore(cfg.max_turns, cfg.session_ttl_minutes)

    api = FastAPI(
        title="MAZ Core",
        version=core_version(),
        dependencies=[Depends(security.authorize)],
    )
    api.add_middleware(
        CORSMiddleware,
        allow_origins=cfg.web_origin_list,
        allow_credentials=False,
        allow_methods=["GET", "POST", "OPTIONS"],
        allow_headers=["Authorization", "Content-Type", "X-MAZ-Token"],
        expose_headers=["X-MAZ-Width", "X-MAZ-Height", "X-MAZ-Format"],
    )

    @api.on_event("startup")
    def start_bridge() -> None:
        bridge_worker.start()

    @api.on_event("shutdown")
    def stop_bridge() -> None:
        bridge_worker.stop()

    def grounded_messages(session_id: str, text: str) -> list[dict[str, str]]:
        history = sessions.messages(session_id)
        if not history and not sessions.has(session_id):
            raise HTTPException(404, "session_not_found")

        context = ""
        if cfg.core_enabled:
            try:
                context += core_service.context_for_prompt(text)
            except (CoreError, OSError, ValueError) as error:
                context += f"\nMAZ Core evidence unavailable: {error}. Say this plainly if relevant."

        if any(word in text.lower() for word in ("agent", "sync", "nudge", "stale", "working")):
            try:
                context += "\nAgent Nudge factual evidence:\n" + json.dumps(nudge_client.summary())
            except (RuntimeError, httpx.HTTPError):
                context += "\nAgent Nudge is unavailable; say that plainly."

        return [
            {"role": "system", "content": SYSTEM_PROMPT + context},
            *history,
            {"role": "user", "content": text},
        ]

    def deterministic_command(session_id: str, text: str, command: dict, actions: list) -> dict:
        if command["type"] == "reminder.create":
            reply = f"Reminder set: {command['title']}"
            provider = "deterministic-local"
        elif command["type"] == "pc.action":
            try:
                result = pc_controller.perform(command["action"])
            except RuntimeError as error:
                raise HTTPException(503, str(error)) from error
            reply = f"PC: {result.label}"
            provider = "pc-local"
        else:
            raise HTTPException(400, "unsupported_command")

        sessions.add_turn(session_id, text, reply)
        return {
            "text": text,
            "reply": reply,
            "provider": provider,
            "actions": actions,
            "commands": [command],
            "timings": {"llm_ms": 0},
        }

    def answer(session_id: str, text: str, route: Route) -> dict:
        refined = refine(text)
        command = parse_command(refined.text)
        if command:
            return deterministic_command(session_id, refined.text, command, refined.actions)
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
        core_status = core_service.status() if cfg.core_enabled else {"ok": False, "disabled": True}
        return {
            "ok": True,
            "version": core_version(),
            "build_id": build_id(),
            "stt": speech.available(),
            "llm": model_router.status(),
            "nudge": nudge_client.status(),
            "tts": speech_out.available(),
            "pc_control": pc_controller.available,
            "core": core_status,
            "bridge": bridge_worker.status(),
        }

    @api.get("/models")
    def models_status():
        return {**model_router.status(), "default_route": cfg.default_route}

    @api.get("/core/status")
    def core_status():
        return core_service.status()

    @api.get("/core/projects")
    def core_projects():
        return {"ok": True, "projects": core_service.projects()}

    @api.get("/core/project/{name}")
    def core_project(name: str):
        try:
            return {"ok": True, "project": core_service.project(name)}
        except CoreError as error:
            raise HTTPException(404, str(error)) from error

    @api.get("/core/search")
    def core_search(query: str, project: str = ""):
        try:
            return core_service.search(query, project)
        except CoreError as error:
            raise HTTPException(400, str(error)) from error

    @api.get("/core/file")
    def core_file(project: str, path: str):
        try:
            return core_service.read_file(project, path)
        except CoreError as error:
            raise HTTPException(400, str(error)) from error

    @api.post("/core/action")
    def core_action(body: CoreActionRequest):
        try:
            return core_service.action(body.action, body.project)
        except CoreError as error:
            raise HTTPException(400, str(error)) from error

    @api.post("/core/job")
    def core_job_start(body: CoreActionRequest):
        try:
            return core_jobs.start(body.action, body.project)
        except CoreError as error:
            raise HTTPException(400, str(error)) from error

    @api.get("/core/job/{job_id}")
    def core_job_status(job_id: str):
        try:
            return core_jobs.status(job_id)
        except CoreError as error:
            raise HTTPException(404, str(error)) from error

    @api.get("/core/jobs")
    def core_jobs_recent(limit: int = 20):
        return {"ok": True, "jobs": core_jobs.recent(limit)}

    @api.get("/core/cardputer/status")
    def core_cardputer_status():
        return core_service.cardputer_status()

    @api.get("/core/cardputer/screen")
    def core_cardputer_screen():
        try:
            frame = core_service.cardputer_screen()
        except CoreError as error:
            raise HTTPException(503, str(error)) from error
        return Response(
            content=frame,
            media_type="application/octet-stream",
            headers={
                "X-MAZ-Width": "240",
                "X-MAZ-Height": "135",
                "X-MAZ-Format": "RGB565LE",
                "Cache-Control": "no-store",
            },
        )

    @api.get("/device")
    def device_status():
        return device_monitor.status()

    @api.get("/device/logs")
    def device_logs(limit: int = 100):
        return {"lines": device_monitor.logs(max(1, min(limit, 500))}

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

    @api.post("/pc/action")
    def pc_action(body: PCActionRequest):
        try:
            result = pc_controller.perform(body.action)
        except RuntimeError as error:
            raise HTTPException(503, str(error)) from error
        return {
            "ok": True,
            "action": result.action,
            "reply": result.label,
            "provider": "pc-local",
        }

    @api.post("/speak")
    def speak(body: SpeakRequest, background_tasks: BackgroundTasks):
        try:
            path = speech_out.synthesize(body.text)
        except RuntimeError as error:
            raise HTTPException(503, str(error)) from error
        background_tasks.add_task(path.unlink, missing_ok=True)
        return FileResponse(path, media_type="audio/wav", filename="maz-reply.wav")

    @api.post("/turn")
    async def turn_audio(
        audio: Annotated[UploadFile, File()],
        session_id: Annotated[str, Form()],
        route: Annotated[Route, Form()] = "local",
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
        x_maz_route: Annotated[Route, Header()] = "local",
    ):
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

    @api.post("/transcribe/raw")
    async def transcribe_raw(request: Request):
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
            started = time.perf_counter()
            text = speech.transcribe(path)
            return {"transcript": text, "stt_ms": round((time.perf_counter() - started) * 1000)}
        finally:
            path.unlink(missing_ok=True)

    @api.post("/extract")
    def extract(body: ExtractRequest):
        prompt = EXTRACT_PROMPTS[body.kind]
        try:
            reply, provider = model_router.chat(
                [
                    {"role": "system", "content": prompt + " Return JSON only."},
                    {"role": "user", "content": body.text},
                ],
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
            text = speech.transcribe(path)
            return structure_braindump(text, highlights, model_router, cfg.default_route)
        finally:
            path.unlink(missing_ok=True)

    return api


app = create_app()
