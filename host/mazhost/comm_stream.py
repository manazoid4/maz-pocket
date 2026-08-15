"""Low-latency COMM WebSocket transport.

Protocol v1 is intentionally tiny and deterministic:
- JSON start/control frames with stable turn/session IDs.
- Binary PCM16LE mono 16 kHz frames, normally exactly 20 ms / 640 bytes.
- Transcript, token deltas and final result are JSON events carrying the same IDs.
- The Cardputer always keeps its local WAV; a broken stream falls back to REST.

The server never buffers a turn in RAM: PCM is written directly to a temporary
WAV and all queues are bounded.
"""

from __future__ import annotations

import asyncio
import hmac
import json
import queue
import re
import tempfile
import threading
import time
import uuid
import wave
from pathlib import Path
from typing import Any, Callable

import httpx
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

from .commands import parse_command
from .llm import Route
from .refine import refine

SAMPLE_RATE = 16_000
CHANNELS = 1
SAMPLE_WIDTH = 2
FRAME_MS = 20
FRAME_BYTES = SAMPLE_RATE * SAMPLE_WIDTH * FRAME_MS // 1000  # 640
PROTOCOL = 1
INCOMING_CAPACITY = 80
TOKEN_CAPACITY = 16
ID_RE = re.compile(r"^[A-Za-z0-9._:-]{1,80}$")

_metrics_lock = threading.Lock()
_metrics: dict[str, int] = {
    "active": 0,
    "accepted": 0,
    "cancelled": 0,
    "disconnects": 0,
    "protocol_errors": 0,
    "last_audio_bytes": 0,
    "last_stt_ms": 0,
    "last_first_token_ms": 0,
    "last_total_ms": 0,
    "incoming_high_water": 0,
}


class ProtocolError(ValueError):
    pass


def _metric(name: str, value: int | None = None, delta: int = 0) -> None:
    with _metrics_lock:
        if value is not None:
            _metrics[name] = value
        elif delta:
            _metrics[name] = _metrics.get(name, 0) + delta


def stream_metrics() -> dict[str, int]:
    with _metrics_lock:
        return dict(_metrics)


def validate_start(payload: dict[str, Any]) -> dict[str, str]:
    if payload.get("type") != "start":
        raise ProtocolError("start_required")
    turn_id = str(payload.get("turn_id", "")).strip()
    if not ID_RE.fullmatch(turn_id):
        raise ProtocolError("invalid_turn_id")
    session_id = str(payload.get("session_id", "")).strip()
    if session_id and not ID_RE.fullmatch(session_id):
        raise ProtocolError("invalid_session_id")
    route = str(payload.get("route", "local"))
    if route not in {"local", "auto", "cloud"}:
        raise ProtocolError("invalid_route")
    audio = payload.get("audio") or {}
    if (
        audio.get("format") != "pcm_s16le"
        or int(audio.get("sample_rate", 0)) != SAMPLE_RATE
        or int(audio.get("channels", 0)) != CHANNELS
        or int(audio.get("frame_ms", 0)) != FRAME_MS
    ):
        raise ProtocolError("unsupported_audio_format")
    return {"turn_id": turn_id, "session_id": session_id, "route": route}


def _json_text(message: dict[str, Any]) -> dict[str, Any]:
    text = message.get("text")
    if not isinstance(text, str):
        raise ProtocolError("text_control_required")
    try:
        value = json.loads(text)
    except json.JSONDecodeError as error:
        raise ProtocolError("invalid_json") from error
    if not isinstance(value, dict):
        raise ProtocolError("json_object_required")
    return value


def install_comm_stream(
    api: FastAPI,
    *,
    cfg: Any,
    security: Any,
    speech: Any,
    model_router: Any,
    sessions: Any,
    grounded_messages: Callable[[str, str], list[dict[str, str]]],
    deterministic_command: Callable[[str, str, dict, list], dict],
) -> None:
    @api.get("/metrics")
    def metrics() -> dict[str, Any]:
        return {"ok": True, "comm_stream": stream_metrics()}

    @api.websocket("/ws/comm")
    async def comm(websocket: WebSocket) -> None:
        # WebSocket routes need an explicit handshake check. Keep the same
        # bearer token as REST; never put the token in the URL/query string.
        try:
            security.require_configured()
        except RuntimeError:
            await websocket.close(code=4403)
            return
        expected = f"Bearer {cfg.token}"
        supplied = websocket.headers.get("authorization", "")
        if not hmac.compare_digest(supplied, expected):
            await websocket.close(code=4401)
            return

        await websocket.accept()
        _metric("active", delta=1)
        _metric("accepted", delta=1)
        started_total = time.perf_counter()
        incoming: asyncio.Queue[dict[str, Any]] = asyncio.Queue(maxsize=INCOMING_CAPACITY)
        cancel_flag = threading.Event()
        disconnected = asyncio.Event()
        receive_error: list[str] = []
        receiver: asyncio.Task[None] | None = None
        path: Path | None = None
        wav: wave.Wave_write | None = None
        turn_id = ""
        session_id = ""

        async def send(event_type: str, **data: Any) -> None:
            payload = {
                "type": event_type,
                "protocol": PROTOCOL,
                "turn_id": turn_id,
                "session_id": session_id,
                **data,
            }
            await websocket.send_text(json.dumps(payload, separators=(",", ":")))

        async def receive_loop() -> None:
            try:
                while True:
                    message = await websocket.receive()
                    if message.get("type") == "websocket.disconnect":
                        break
                    if message.get("text") is not None:
                        try:
                            control = _json_text(message)
                        except ProtocolError as error:
                            receive_error.append(str(error))
                            break
                        if control.get("type") == "cancel":
                            incoming_turn = str(control.get("turn_id", ""))
                            if not incoming_turn or not turn_id or incoming_turn == turn_id:
                                cancel_flag.set()
                                continue
                        item = {"control": control}
                    else:
                        data = message.get("bytes")
                        if data is None:
                            continue
                        item = {"bytes": data}
                    try:
                        incoming.put_nowait(item)
                        _metric(
                            "incoming_high_water",
                            value=max(stream_metrics()["incoming_high_water"], incoming.qsize()),
                        )
                    except asyncio.QueueFull:
                        receive_error.append("incoming_queue_full")
                        break
            except WebSocketDisconnect:
                pass
            except RuntimeError as error:
                receive_error.append(str(error))
            finally:
                disconnected.set()

        try:
            receiver = asyncio.create_task(receive_loop())

            # First non-cancel control must be START.
            try:
                first = await asyncio.wait_for(incoming.get(), timeout=8.0)
            except asyncio.TimeoutError as error:
                raise ProtocolError("start_timeout") from error
            if "control" not in first:
                raise ProtocolError("start_required")
            start = validate_start(first["control"])
            turn_id = start["turn_id"]
            route: Route = start["route"]  # type: ignore[assignment]
            requested_session = start["session_id"]
            if requested_session and sessions.has(requested_session):
                session_id = requested_session
            else:
                session_id = sessions.start()

            temp = tempfile.NamedTemporaryFile(delete=False, suffix=".wav")
            path = Path(temp.name)
            temp.close()
            wav = wave.open(str(path), "wb")
            wav.setnchannels(CHANNELS)
            wav.setsampwidth(SAMPLE_WIDTH)
            wav.setframerate(SAMPLE_RATE)
            await send(
                "ready",
                audio={
                    "format": "pcm_s16le",
                    "sample_rate": SAMPLE_RATE,
                    "channels": CHANNELS,
                    "frame_ms": FRAME_MS,
                },
            )

            audio_bytes = 0
            max_bytes = int(cfg.max_audio_seconds) * SAMPLE_RATE * SAMPLE_WIDTH
            ended = False
            while not ended:
                if cancel_flag.is_set():
                    _metric("cancelled", delta=1)
                    await send("cancelled")
                    return
                if receive_error:
                    raise ProtocolError(receive_error[0])
                if disconnected.is_set() and incoming.empty():
                    _metric("disconnects", delta=1)
                    return
                try:
                    item = await asyncio.wait_for(incoming.get(), timeout=0.5)
                except asyncio.TimeoutError:
                    continue
                if "bytes" in item:
                    data = item["bytes"]
                    if not data or len(data) % SAMPLE_WIDTH:
                        raise ProtocolError("invalid_pcm_frame")
                    audio_bytes += len(data)
                    if audio_bytes > max_bytes:
                        raise ProtocolError("audio_too_long")
                    wav.writeframesraw(data)
                    continue
                control = item["control"]
                ctype = control.get("type")
                if ctype == "end":
                    if str(control.get("turn_id", turn_id)) != turn_id:
                        raise ProtocolError("turn_id_mismatch")
                    ended = True
                elif ctype == "ping":
                    await send("pong")
                elif ctype != "start":
                    raise ProtocolError("unknown_control")

            wav.close()
            wav = None
            _metric("last_audio_bytes", value=audio_bytes)
            if audio_bytes == 0:
                raise ProtocolError("empty_audio")
            if cancel_flag.is_set():
                _metric("cancelled", delta=1)
                await send("cancelled")
                return

            stt_started = time.perf_counter()
            transcript = await asyncio.to_thread(speech.transcribe, path)
            stt_ms = round((time.perf_counter() - stt_started) * 1000)
            _metric("last_stt_ms", value=stt_ms)
            await send("transcript", text=transcript, stt_ms=stt_ms)

            refined = refine(transcript)
            command = parse_command(refined.text)
            if command:
                result = await asyncio.to_thread(
                    deterministic_command,
                    session_id,
                    refined.text,
                    command,
                    refined.actions,
                )
                total_ms = round((time.perf_counter() - started_total) * 1000)
                _metric("last_first_token_ms", value=0)
                _metric("last_total_ms", value=total_ms)
                await send(
                    "done",
                    reply=result["reply"],
                    provider=result["provider"],
                    commands=result.get("commands", []),
                    actions=result.get("actions", []),
                    timings={"stt_ms": stt_ms, "first_token_ms": 0, "total_ms": total_ms},
                )
                return

            messages = await asyncio.to_thread(grounded_messages, session_id, refined.text)
            token_queue: queue.Queue[tuple[str, str] | BaseException | None] = queue.Queue(
                maxsize=TOKEN_CAPACITY
            )

            def produce() -> None:
                try:
                    for chunk, provider in model_router.stream_chat(messages, route):
                        if cancel_flag.is_set():
                            break
                        token_queue.put((chunk, provider))
                except BaseException as error:  # moved back to event loop below
                    token_queue.put(error)
                finally:
                    token_queue.put(None)

            producer = asyncio.create_task(asyncio.to_thread(produce))
            reply_parts: list[str] = []
            provider = ""
            first_token_ms = 0
            llm_started = time.perf_counter()
            while True:
                if cancel_flag.is_set():
                    _metric("cancelled", delta=1)
                    await send("cancelled")
                    await producer
                    return
                item = await asyncio.to_thread(token_queue.get)
                if item is None:
                    break
                if isinstance(item, BaseException):
                    raise RuntimeError(str(item)) from item
                chunk, provider = item
                if not chunk:
                    continue
                if not first_token_ms:
                    first_token_ms = round((time.perf_counter() - llm_started) * 1000)
                reply_parts.append(chunk)
                await send("delta", text=chunk)

            await producer
            reply = "".join(reply_parts).strip()
            if not reply:
                raise RuntimeError("local_model_empty_reply")
            sessions.add_turn(session_id, refined.text, reply)
            total_ms = round((time.perf_counter() - started_total) * 1000)
            _metric("last_first_token_ms", value=first_token_ms)
            _metric("last_total_ms", value=total_ms)
            await send(
                "done",
                reply=reply,
                provider=provider,
                actions=refined.actions,
                timings={
                    "stt_ms": stt_ms,
                    "first_token_ms": first_token_ms,
                    "llm_ms": round((time.perf_counter() - llm_started) * 1000),
                    "total_ms": total_ms,
                },
            )
        except ProtocolError as error:
            _metric("protocol_errors", delta=1)
            try:
                await send("error", error=str(error), fallback="rest")
            except RuntimeError:
                pass
        except (RuntimeError, httpx.HTTPError, ValueError) as error:
            try:
                await send("error", error=str(error), fallback="rest")
            except RuntimeError:
                pass
        finally:
            cancel_flag.set()
            if receiver:
                receiver.cancel()
            if wav is not None:
                try:
                    wav.close()
                except (OSError, wave.Error):
                    pass
            if path is not None:
                path.unlink(missing_ok=True)
            _metric("active", delta=-1)
