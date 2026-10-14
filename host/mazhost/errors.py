from __future__ import annotations

from enum import Enum


class ErrorCode(str, Enum):
    AUTH_FAILED = "AUTH_FAILED"
    MODEL_NOT_FOUND = "MODEL_NOT_FOUND"
    ENDPOINT_NOT_FOUND = "ENDPOINT_NOT_FOUND"
    RATE_LIMITED = "RATE_LIMITED"
    UPSTREAM_TIMEOUT = "UPSTREAM_TIMEOUT"
    UPSTREAM_UNAVAILABLE = "UPSTREAM_UNAVAILABLE"
    INVALID_UPSTREAM_RESPONSE = "INVALID_UPSTREAM_RESPONSE"
    LOCAL_UNAVAILABLE = "LOCAL_UNAVAILABLE"


class RouteError(RuntimeError):
    def __init__(
        self,
        code: ErrorCode,
        route: str,
        *,
        retryable: bool,
        upstream_status: int | None = None,
    ) -> None:
        super().__init__(code.value)
        self.code = code
        self.route = route
        self.retryable = retryable
        self.upstream_status = upstream_status

    @property
    def http_status(self) -> int:
        if self.code == ErrorCode.AUTH_FAILED:
            return 502
        if self.code == ErrorCode.MODEL_NOT_FOUND:
            return 503
        if self.code == ErrorCode.RATE_LIMITED:
            return 503
        if self.code == ErrorCode.INVALID_UPSTREAM_RESPONSE:
            return 502
        return 503

    def payload(self) -> dict:
        payload = {
            "ok": False,
            "error": self.code.value,
            "route": self.route,
            "retryable": self.retryable,
        }
        if self.upstream_status is not None:
            payload["upstream_status"] = self.upstream_status
        return payload

