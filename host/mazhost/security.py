from __future__ import annotations

import hmac
import ipaddress
import threading
import time
import wave
from pathlib import Path

from fastapi import Header, HTTPException, Request, status
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.responses import JSONResponse

from .config import Settings


class Security:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def require_configured(self) -> None:
        if not self.settings.token_configured:
            raise RuntimeError("Set MAZ_TOKEN to a non-default random value before starting MAZ Host")

    def token_ok(self, authorization: str | None) -> bool:
        if not self.settings.token_configured or not authorization:
            return False
        return hmac.compare_digest(authorization, f"Bearer {self.settings.token}")

    def authorize(self, request: Request, authorization: str | None = Header(default=None)) -> None:
        self.require_configured()
        if request.url.path in PUBLIC_PATHS:
            return  # /health: handler returns minimal info unless token is valid
        if not self.token_ok(authorization):
            raise HTTPException(status.HTTP_401_UNAUTHORIZED, "authentication_required")

    def validate_upload(self, path: Path, size: int) -> float:
        if size > self.settings.max_upload_mb * 1024 * 1024:
            raise HTTPException(status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, "audio_too_large")
        try:
            with wave.open(str(path), "rb") as wav:
                if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
                    raise HTTPException(status.HTTP_400_BAD_REQUEST, "unsupported_wav_format")
                duration = wav.getnframes() / max(wav.getframerate(), 1)
        except (wave.Error, EOFError) as error:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "invalid_wav") from error
        if duration > self.settings.max_audio_seconds:
            raise HTTPException(status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, "audio_too_long")
        return duration


# --- remote exposure (Tailscale Funnel etc.) --------------------------------

PUBLIC_PATHS = frozenset({"/health", "/ui"})  # /ui: static page only, holds no data
_DOC_PREFIXES = ("/docs", "/redoc", "/openapi.json", "/pair/docs", "/pair/redoc", "/pair/openapi.json")
FUNNEL_HEADER = "tailscale-funnel-request"


def _parse_ip(value: str):
    try:
        return ipaddress.ip_address(value.strip().split("%")[0])
    except ValueError:
        return None


def is_lan_ip(value: str) -> bool:
    """Loopback, RFC1918, link-local, ULA and CGNAT (tailnet) count as LAN."""
    ip = _parse_ip(value)
    if ip is None:
        return False
    if getattr(ip, "ipv4_mapped", None):
        ip = ip.ipv4_mapped
    return bool(
        ip.is_loopback
        or ip.is_private
        or ip.is_link_local
        or (ip.version == 4 and ip in ipaddress.ip_network("100.64.0.0/10"))
    )


def client_info(request: Request) -> tuple[str, bool, bool]:
    """Return (effective_ip, is_remote, is_true_loopback).

    Forwarding headers are honored ONLY when the TCP peer is loopback, which is
    how tailscaled hands Funnel traffic to Core (it sets X-Forwarded-For and
    Tailscale-Funnel-Request). A direct LAN/internet peer cannot spoof them.
    """
    peer = request.client.host if request.client else ""
    peer_ip = _parse_ip(peer)
    if peer_ip is not None and getattr(peer_ip, "ipv4_mapped", None):
        peer_ip = peer_ip.ipv4_mapped
    if peer_ip is not None and peer_ip.is_loopback:
        xff = request.headers.get("x-forwarded-for", "")
        funnel = request.headers.get(FUNNEL_HEADER) is not None
        if xff or funnel:
            real = xff.split(",")[-1].strip() if xff else ""
            if _parse_ip(real) is None:
                real = "funnel-unknown"
            return real, True, False
        return peer, False, True
    if peer_ip is None:
        return peer or "unknown", False, False  # unix socket / in-process test client
    return peer, not is_lan_ip(peer), False


class AuthFailureLimiter:
    """Per-IP sliding window of auth failures; locks the IP out once exceeded."""

    def __init__(self, max_failures: int = 10, window_s: float = 60.0, lockout_s: float = 60.0, clock=time.monotonic) -> None:
        self.max_failures, self.window_s, self.lockout_s, self.clock = max_failures, window_s, lockout_s, clock
        self._fails: dict[str, list[float]] = {}
        self._locked: dict[str, float] = {}
        self._lock = threading.Lock()

    def retry_after(self, ip: str) -> int:
        with self._lock:
            until = self._locked.get(ip, 0.0)
            now = self.clock()
            if until > now:
                return max(1, int(until - now) + 1)
            self._locked.pop(ip, None)
            return 0

    def record_failure(self, ip: str) -> None:
        with self._lock:
            now = self.clock()
            if len(self._fails) > 5000:
                self._fails.clear()
            hits = [t for t in self._fails.get(ip, []) if now - t < self.window_s] + [now]
            self._fails[ip] = hits
            if len(hits) >= self.max_failures:
                self._locked[ip] = now + self.lockout_s
                self._fails.pop(ip, None)


class RemoteGuardMiddleware(BaseHTTPMiddleware):
    def __init__(self, app, limiter: AuthFailureLimiter | None = None) -> None:
        super().__init__(app)
        self.limiter = limiter or AuthFailureLimiter()

    async def dispatch(self, request: Request, call_next):
        ip, remote, loopback = client_info(request)
        path = request.url.path
        if remote and request.headers.get("x-forwarded-proto", "").lower() == "https":
            request.scope["scheme"] = "https"  # so the phone session cookie gets Secure
        if remote:
            if path.startswith(_DOC_PREFIXES):
                return JSONResponse({"detail": "not_found"}, status_code=404)
            if path.startswith("/pair") and path.rstrip("/") != "/pair/start":
                return JSONResponse({"detail": "pairing_is_local_only"}, status_code=403)
        if not loopback:
            wait = self.limiter.retry_after(ip)
            if wait:
                return JSONResponse({"detail": "too_many_auth_failures"}, status_code=429, headers={"Retry-After": str(wait)})
        response = await call_next(request)
        if not loopback and response.status_code in (401, 403) and path != "/health":
            self.limiter.record_failure(ip)
        elif not loopback and response.status_code == 400 and path == "/pair/claim":
            self.limiter.record_failure(ip)
        return response
