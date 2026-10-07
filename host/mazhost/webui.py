"""Web UI v0: one static page (dump inbox + browser recorder). Data calls stay token-protected."""
from __future__ import annotations

from pathlib import Path

from fastapi import APIRouter
from fastapi.responses import HTMLResponse

router = APIRouter()
PAGE = Path(__file__).parent / "static" / "ui.html"


@router.get("/ui", response_class=HTMLResponse, include_in_schema=False)
def ui() -> HTMLResponse:
    # Public path (see security.PUBLIC_PATHS): the page holds no data; every fetch it makes sends the token.
    return HTMLResponse(
        PAGE.read_text(encoding="utf-8"),
        headers={"Cache-Control": "no-store", "Referrer-Policy": "no-referrer"},
    )
