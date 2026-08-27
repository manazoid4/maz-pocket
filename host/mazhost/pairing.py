from __future__ import annotations

import hashlib
import secrets
import time
from dataclasses import dataclass

from fastapi import Depends, FastAPI, HTTPException, Request, status
from fastapi.responses import HTMLResponse
from pydantic import BaseModel, Field

from .config import Settings
from .security import Security

# Crockford-ish alphabet: no 0/O/1/I/L, so a human reading this off a small
# screen and typing it on a phone keyboard can't confuse look-alike chars.
CODE_ALPHABET = "23456789ABCDEFGHJKMNPQRSTUVWXYZ"
CODE_LENGTH = 8
CODE_TTL_SECONDS = 300
MAX_CLAIM_ATTEMPTS_PER_CODE = 8
MAX_CLAIMS_PER_MINUTE_PER_IP = 10


PAIR_PAGE = r"""<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#080b0e"><title>Pair with MAZ</title><style>
:root{color-scheme:dark}body{margin:0;background:#080b0e;color:#f2f4f5;font:15px ui-monospace,Consolas,monospace;display:grid;min-height:100vh;place-items:center}.c{box-sizing:border-box;width:min(92vw,420px);border:1px solid #27323b;background:#11171c;border-radius:12px;padding:20px}b{color:#ffcc00}p{color:#aeb9c1;line-height:1.5}input,button{box-sizing:border-box;width:100%;font:inherit;padding:13px;border-radius:8px;border:1px solid #394852;background:#080c0f;color:#f2f4f5;margin-top:10px;text-transform:uppercase;letter-spacing:.12em}button{background:#ffcc00;color:#08090a;font-weight:900}.ok{color:#62d98b}.bad{color:#ff7676}small{color:#7f909b}</style></head>
<body><form class="c" id="pair"><h1><b>MAZ</b> PAIR DEVICE</h1><p>On the Cardputer open <b>CONTROL → PAIRING + PHONE</b>, press Enter, then type the temporary 8-character code below.</p><input id="code" inputmode="text" autocomplete="one-time-code" maxlength="8" pattern="[A-Za-z2-9]{8}" placeholder="8-CHAR CODE" required autofocus><button>PAIR THIS PHONE</button><p id="status"><small>The code expires in 5 minutes and works once.</small></p></form><script>
const f=document.getElementById('pair'),s=document.getElementById('status'),c=document.getElementById('code');
f.addEventListener('submit',async e=>{e.preventDefault();s.textContent='Pairing…';try{const r=await fetch('claim',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({code:c.value.trim().toUpperCase()})});const d=await r.json();if(!r.ok)throw new Error('Code invalid, expired, or already used.');s.className='ok';s.textContent='Paired. Opening MAZ WORK…';const login=document.createElement('form');login.method='post';login.action='/control/session/login';const token=document.createElement('input');token.type='hidden';token.name='token';token.value=d.token;login.appendChild(token);document.body.appendChild(login);login.submit()}catch(err){s.className='bad';s.textContent=err.message;c.select()}});
</script></body></html>"""


def _generate_code() -> str:
    return "".join(secrets.choice(CODE_ALPHABET) for _ in range(CODE_LENGTH))


def _hash_code(code: str) -> str:
    return hashlib.sha256(code.encode("utf-8")).hexdigest()


@dataclass
class _Pending:
    expires_at: float
    attempts: int = 0
    used: bool = False


class PairingStore:
    """In-memory, single-process, single-outstanding-code pairing state.

    One hot pairing window at a time is enough for a handheld pairing flow
    (someone is standing at the host, watching the code, pairing one new
    client) and keeps this free of any persistence layer. Starting a new
    pairing invalidates whatever code was previously outstanding, so a code
    displayed once and forgotten can never be claimed later.
    """

    def __init__(self) -> None:
        # Exactly one outstanding pairing session at a time. The attempt
        # budget below is charged to *this session*, not to whichever digest
        # a guess happens to hash to — a wrong guess must burn down the
        # budget exactly like a right one, or the attempt cap does nothing
        # against brute force (a guess that doesn't match anything was
        # never touching the real entry's counter).
        self._current_digest: str | None = None
        self._current: _Pending | None = None
        self._claim_attempts_by_ip: dict[str, list[float]] = {}

    def start(self) -> tuple[str, int]:
        code = _generate_code()
        self._current_digest = _hash_code(code)
        self._current = _Pending(expires_at=time.monotonic() + CODE_TTL_SECONDS)
        return code, CODE_TTL_SECONDS

    def _rate_limited(self, client_ip: str) -> bool:
        now = time.monotonic()
        recent = [t for t in self._claim_attempts_by_ip.get(client_ip, []) if now - t < 60]
        recent.append(now)
        self._claim_attempts_by_ip[client_ip] = recent
        return len(recent) > MAX_CLAIMS_PER_MINUTE_PER_IP

    def claim(self, code: str, client_ip: str) -> bool:
        """True iff the code is valid, unused, unexpired, and the caller
        isn't rate-limited. Every failure path returns the same False with
        no distinguishing signal — a claimant must not be able to tell
        "wrong code" from "no code outstanding" from "already used" from
        "rate limited". That distinction is exactly what would let repeated
        guessing narrow down a live credential.
        """
        if self._rate_limited(client_ip):
            return False
        if self._current is None:
            return False
        self._current.attempts += 1
        expired = time.monotonic() > self._current.expires_at
        exhausted = self._current.used or self._current.attempts > MAX_CLAIM_ATTEMPTS_PER_CODE or expired
        matches = _hash_code(code) == self._current_digest
        if exhausted or not matches:
            if exhausted:
                self._current = None
                self._current_digest = None
            return False
        self._current.used = True
        self._current = None
        self._current_digest = None
        return True


class ClaimRequest(BaseModel):
    code: str = Field(min_length=1, max_length=32)


def build_pairing_app(
    settings: Settings, security: Security, store: PairingStore | None = None
) -> FastAPI:
    pairing_store = store or PairingStore()
    pair_app = FastAPI(title="MAZ Pairing")

    @pair_app.get("/", response_class=HTMLResponse)
    def home() -> HTMLResponse:
        # Public by design: the page contains no credential and a claimant
        # still needs the short-lived, rate-limited, single-use device code.
        return HTMLResponse(PAIR_PAGE)

    @pair_app.post("/start", dependencies=[Depends(security.authorize)])
    def start() -> dict:
        # Minting a code requires the real long-lived credential already —
        # this is how an *authenticated* host/Cardputer session invites a
        # *new* client in, not a way to obtain the first credential.
        code, ttl = pairing_store.start()
        return {"code": code, "expires_in_seconds": ttl}

    @pair_app.post("/claim")
    def claim(body: ClaimRequest, request: Request) -> dict:
        client_ip = request.client.host if request.client else "unknown"
        normalized = body.code.strip().upper()
        if not pairing_store.claim(normalized, client_ip):
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "invalid_or_expired_code")
        return {"token": settings.token}

    return pair_app
