from __future__ import annotations

from dataclasses import dataclass
from enum import StrEnum
from typing import Any

import httpx


class ErrorCode(StrEnum):
    AUTH_FAILED = "AUTH_FAILED"
    MODEL_NOT_FOUND = "MODEL_NOT_FOUND"
    ENDPOINT_NOT_FOUND = "ENDPOINT_NOT_FOUND"
    RATE_LIMITED = "RATE_LIMITED"
    UPSTREAM_TIMEOUT = "UPSTREAM_TIMEOUT"
    UPSTREAM_UNAVAILABLE = "UPSTREAM_UNAVAILABLE"
    LOCAL_UNAVAILABLE = "LOCAL_UNAVAILABLE"
    INVALID_UPSTREAM_RESPONSE = "INVALID_UPSTREAM_RESPONSE"
    ROUTE_UNAVAILABLE = "ROUTE_UNAVAILABLE"
    INTERNAL_ERROR = "INTERNAL_ERROR"


HTTP_STATUS_BY_CODE = {
    ErrorCode.AUTH_FAILED: 401,
    ErrorCode.MODEL_NOT_FOUND: 404,
    ErrorCode.ENDPOINT_NOT_FOUND: 404,
    ErrorCode.RATE_LIMITED: 429,
    ErrorCode.UPSTREAM_TIMEOUT: 504,
    ErrorCode.UPSTREAM_UNAVAILABLE: 503,
    ErrorCode.LOCAL_UNAVAILABLE: 503,
    ErrorCode.INVALID_UPSTREAM_RESPONSE: 502,
    ErrorCode.ROUTE_UNAVAILABLE: 503,
    ErrorCode.INTERNAL_ERROR: 500,
}


@dataclass(slots=True)
class RouteError(Exception):
    code: ErrorCode
    route: str
    upstream_status: int | None = None
    retryable: bool = False
    requested_route: str | None = None
    model: str | None = None

    @property
    def http_status(self) -> int:
        return HTTP_STATUS_BY_CODE[self.code]

    def payload(self) -> dict[str, Any]:
        result: dict[str, Any] = {
            "ok": False,
            "route": self.route,
            "error": self.code.value,
            "retryable": self.retryable,
        }
        if self.requested_route and self.requested_route != self.route:
            result["requested_route"] = self.requested_route
        if self.upstream_status is not None:
            result["upstream_status"] = self.upstream_status
        if self.model:
            result["model"] = self.model
        return result


def _response_mentions_model(response: httpx.Response) -> bool:
    try:
        body = response.text[:2_000].lower()
    except (httpx.HTTPError, UnicodeError):
        return False
    return "model" in body or "deployment" in body


def normalize_upstream_error(
    error: Exception,
    *,
    route: str,
    model: str | None = None,
    local: bool = False,
) -> RouteError:
    if isinstance(error, RouteError):
        return error
    if isinstance(error, httpx.TimeoutException):
        return RouteError(ErrorCode.UPSTREAM_TIMEOUT, route, retryable=True, model=model)
    if isinstance(error, httpx.ConnectError):
        code = ErrorCode.LOCAL_UNAVAILABLE if local else ErrorCode.UPSTREAM_UNAVAILABLE
        return RouteError(code, route, retryable=True, model=model)
    if isinstance(error, httpx.HTTPStatusError):
        status = error.response.status_code
        if status in (401, 403):
            code, retryable = ErrorCode.AUTH_FAILED, False
        elif status == 404:
            code = ErrorCode.MODEL_NOT_FOUND if _response_mentions_model(error.response) else ErrorCode.ENDPOINT_NOT_FOUND
            retryable = False
        elif status == 429:
            code, retryable = ErrorCode.RATE_LIMITED, True
        elif status >= 500:
            code, retryable = ErrorCode.UPSTREAM_UNAVAILABLE, True
        else:
            code, retryable = ErrorCode.ROUTE_UNAVAILABLE, False
        return RouteError(code, route, upstream_status=status, retryable=retryable, model=model)
    if isinstance(error, (KeyError, TypeError, ValueError)):
        return RouteError(ErrorCode.INVALID_UPSTREAM_RESPONSE, route, retryable=False, model=model)
    return RouteError(ErrorCode.INTERNAL_ERROR, route, retryable=False, model=model)
