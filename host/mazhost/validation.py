from __future__ import annotations

import math
from typing import Any

from fastapi import FastAPI, Request
from fastapi.encoders import jsonable_encoder
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse


def _json_safe(value: Any) -> Any:
    """Convert validation evidence to standards-compliant JSON values."""
    if isinstance(value, float) and not math.isfinite(value):
        return str(value)
    if isinstance(value, dict):
        return {key: _json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_safe(item) for item in value]
    return value


def install_validation_exception_handler(app: FastAPI) -> None:
    """Return malformed inputs as 422 even when evidence contains NaN/Infinity."""

    @app.exception_handler(RequestValidationError)
    async def validation_exception_handler(
        _request: Request, error: RequestValidationError
    ) -> JSONResponse:
        detail = _json_safe(jsonable_encoder(error.errors()))
        return JSONResponse(status_code=422, content={"detail": detail})
