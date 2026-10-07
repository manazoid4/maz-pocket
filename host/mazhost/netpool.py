"""One keep-alive HTTP client for the speech path (Groq STT, Fish TTS).

A fresh httpx.post per call pays DNS + TCP + TLS (~150-400 ms) every turn. One
shared client keeps the connection open between turns; warm() opens them early.
"""

from __future__ import annotations

import threading

import httpx

client = httpx.Client(limits=httpx.Limits(keepalive_expiry=300))


def warm(*urls: str) -> None:
    """Fire-and-forget: open TLS connections to these hosts in the background."""
    def run() -> None:
        for url in urls:
            try:
                client.head(url, timeout=5)
            except httpx.HTTPError:
                pass
    threading.Thread(target=run, name="netpool-warm", daemon=True).start()
