from __future__ import annotations

import base64
import hashlib
import hmac
import json
import os
import secrets
import threading
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Literal

from .config import Settings


Scope = Literal["read_only", "project_full", "pc_full", "admin"]
VALID_SCOPES: set[str] = {"read_only", "project_full", "pc_full", "admin"}


def _b64(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _unb64(text: str) -> bytes:
    return base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))


@dataclass
class ApprovalRequest:
    request_id: str
    task: str
    agent: str
    scope: Scope
    project: str = ""
    duration_seconds: int = 600
    commands_preview: list[str] = field(default_factory=list)
    files: list[str] = field(default_factory=list)
    network_targets: list[str] = field(default_factory=list)
    requires_admin: bool = False
    created_at: float = field(default_factory=time.time)
    expires_at: float = 0.0
    status: Literal["pending", "approved", "denied", "expired"] = "pending"
    grant_id: str = ""
    grant_token: str = ""


@dataclass
class Grant:
    grant_id: str
    request_id: str
    task: str
    agent: str
    scope: Scope
    project: str
    issued_at: float
    expires_at: float
    requires_admin: bool
    revoked: bool = False


class AuthorityError(RuntimeError):
    pass


class AuthorityBroker:
    """Human approval boundary for elevated MAZ actions.

    The model-facing API may create requests and later present a signed grant,
    but it has no method that approves its own request. Approval is performed by
    the phone control app, which authenticates separately and calls `approve`
    directly inside this process.
    """

    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.root = Path(settings.control_dir).expanduser()
        self.root.mkdir(parents=True, exist_ok=True)
        self._secret = self._load_or_create_secret()
        self._requests: dict[str, ApprovalRequest] = {}
        self._grants: dict[str, Grant] = {}
        self._lock = threading.RLock()
        self._audit_path = self.root / "authority-audit.jsonl"

    # -------------------------------------------------------------- identity
    @property
    def token_id(self) -> str:
        # This is safe to show in UIs. It identifies which MAZ pairing token is
        # configured without revealing the token itself.
        return hashlib.sha256(self.settings.token.encode("utf-8")).hexdigest()[:12]

    def _load_or_create_secret(self) -> bytes:
        path = self.root / "authority.key"
        if path.exists():
            value = path.read_bytes()
            if len(value) >= 32:
                return value
        value = secrets.token_bytes(32)
        tmp = path.with_suffix(".tmp")
        tmp.write_bytes(value)
        try:
            os.chmod(tmp, 0o600)
        except OSError:
            pass
        tmp.replace(path)
        return value

    # --------------------------------------------------------------- signing
    def _sign(self, payload: dict[str, Any]) -> str:
        raw = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
        body = _b64(raw)
        sig = hmac.new(self._secret, body.encode("ascii"), hashlib.sha256).digest()
        return body + "." + _b64(sig)

    def _verify_signed(self, token: str) -> dict[str, Any]:
        try:
            body, supplied = token.split(".", 1)
            expected = hmac.new(self._secret, body.encode("ascii"), hashlib.sha256).digest()
            if not hmac.compare_digest(_unb64(supplied), expected):
                raise AuthorityError("invalid_signature")
            payload = json.loads(_unb64(body))
        except (ValueError, json.JSONDecodeError, UnicodeDecodeError) as error:
            raise AuthorityError("invalid_token") from error
        if float(payload.get("exp", 0)) <= time.time():
            raise AuthorityError("grant_expired")
        return payload

    # ---------------------------------------------------------- phone session
    def issue_phone_session(self, user_agent: str, seconds: int | None = None) -> str:
        ttl = seconds or self.settings.control_phone_session_seconds
        now = time.time()
        payload = {
            "typ": "phone",
            "iat": int(now),
            "exp": int(now + ttl),
            "ua": hashlib.sha256(user_agent.encode("utf-8")).hexdigest()[:16],
            "nonce": secrets.token_hex(12),
        }
        return self._sign(payload)

    def verify_phone_session(self, token: str, user_agent: str) -> None:
        payload = self._verify_signed(token)
        if payload.get("typ") != "phone":
            raise AuthorityError("wrong_token_type")
        ua = hashlib.sha256(user_agent.encode("utf-8")).hexdigest()[:16]
        if not hmac.compare_digest(str(payload.get("ua", "")), ua):
            raise AuthorityError("phone_session_mismatch")

    # -------------------------------------------------------------- requests
    def request(
        self,
        *,
        task: str,
        agent: str,
        scope: Scope,
        project: str = "",
        duration_seconds: int | None = None,
        commands_preview: list[str] | None = None,
        files: list[str] | None = None,
        network_targets: list[str] | None = None,
        requires_admin: bool = False,
    ) -> dict[str, Any]:
        if scope not in VALID_SCOPES:
            raise AuthorityError("invalid_scope")
        if requires_admin and scope != "admin":
            raise AuthorityError("admin_scope_required")
        task = " ".join(task.split())[:400]
        agent = " ".join(agent.split())[:120] or "unknown"
        project = project.strip()[:500]
        if not task:
            raise AuthorityError("task_required")
        if scope == "project_full" and not project:
            raise AuthorityError("project_required")
        ttl = duration_seconds or self.settings.control_default_grant_seconds
        ttl = max(30, min(ttl, self.settings.control_max_grant_seconds))
        now = time.time()
        item = ApprovalRequest(
            request_id="apr_" + secrets.token_urlsafe(12),
            task=task,
            agent=agent,
            scope=scope,
            project=project,
            duration_seconds=ttl,
            commands_preview=[str(x)[:500] for x in (commands_preview or [])[:20]],
            files=[str(x)[:500] for x in (files or [])[:40]],
            network_targets=[str(x)[:300] for x in (network_targets or [])[:20]],
            requires_admin=requires_admin,
            created_at=now,
            expires_at=now + self.settings.control_request_ttl_seconds,
        )
        with self._lock:
            self._requests[item.request_id] = item
        self._audit("request", {k: v for k, v in asdict(item).items() if k != "grant_token"})
        return self.request_status(item.request_id)

    def _expire(self) -> None:
        now = time.time()
        for item in self._requests.values():
            if item.status == "pending" and item.expires_at <= now:
                item.status = "expired"

    def pending(self) -> list[dict[str, Any]]:
        with self._lock:
            self._expire()
            items = [asdict(x) for x in self._requests.values() if x.status == "pending"]
        for item in items:
            item.pop("grant_token", None)
        return sorted(items, key=lambda x: x["created_at"], reverse=True)

    def request_status(self, request_id: str, *, reveal_grant: bool = True) -> dict[str, Any]:
        with self._lock:
            self._expire()
            item = self._requests.get(request_id)
            if not item:
                raise AuthorityError("request_not_found")
            result = asdict(item)
        if not reveal_grant or result.get("status") != "approved":
            result.pop("grant_token", None)
        return result

    def approve(self, request_id: str) -> dict[str, Any]:
        with self._lock:
            self._expire()
            item = self._requests.get(request_id)
            if not item:
                raise AuthorityError("request_not_found")
            if item.status != "pending":
                raise AuthorityError(f"request_{item.status}")
            now = time.time()
            grant = Grant(
                grant_id="gnt_" + secrets.token_urlsafe(12),
                request_id=request_id,
                task=item.task,
                agent=item.agent,
                scope=item.scope,
                project=item.project,
                issued_at=now,
                expires_at=now + item.duration_seconds,
                requires_admin=item.requires_admin,
            )
            payload = {
                "typ": "grant",
                "gid": grant.grant_id,
                "rid": grant.request_id,
                "agent": grant.agent,
                "scope": grant.scope,
                "project": grant.project,
                "admin": grant.requires_admin,
                "iat": int(grant.issued_at),
                "exp": int(grant.expires_at),
                "nonce": secrets.token_hex(12),
            }
            item.status = "approved"
            item.grant_id = grant.grant_id
            item.grant_token = self._sign(payload)
            self._grants[grant.grant_id] = grant
            result = asdict(item)
        self._audit("approved", {k: v for k, v in result.items() if k != "grant_token"})
        return result

    def deny(self, request_id: str) -> dict[str, Any]:
        with self._lock:
            item = self._requests.get(request_id)
            if not item:
                raise AuthorityError("request_not_found")
            if item.status != "pending":
                raise AuthorityError(f"request_{item.status}")
            item.status = "denied"
            result = asdict(item)
            result.pop("grant_token", None)
        self._audit("denied", result)
        return result

    # --------------------------------------------------------------- grants
    def active_grants(self) -> list[dict[str, Any]]:
        now = time.time()
        with self._lock:
            result = [
                asdict(g)
                for g in self._grants.values()
                if not g.revoked and g.expires_at > now
            ]
        return sorted(result, key=lambda x: x["expires_at"])

    def verify_grant(
        self,
        token: str,
        *,
        required: set[str] | None = None,
        agent: str = "",
    ) -> Grant:
        payload = self._verify_signed(token)
        if payload.get("typ") != "grant":
            raise AuthorityError("wrong_token_type")
        grant_id = str(payload.get("gid", ""))
        with self._lock:
            grant = self._grants.get(grant_id)
            if not grant:
                raise AuthorityError("grant_not_found")
            if grant.revoked:
                raise AuthorityError("grant_revoked")
            if grant.expires_at <= time.time():
                raise AuthorityError("grant_expired")
        if agent and grant.agent not in (agent, "crew", "any"):
            raise AuthorityError("wrong_agent")
        if required:
            effective = {grant.scope}
            if grant.scope == "admin":
                effective.update({"pc_full", "project_full", "read_only", "admin"})
            elif grant.scope == "pc_full":
                effective.update({"project_full", "read_only", "pc_full"})
            elif grant.scope == "project_full":
                effective.update({"read_only", "project_full"})
            if not required.issubset(effective):
                raise AuthorityError("insufficient_scope")
        return grant

    def revoke(self, grant_id: str) -> dict[str, Any]:
        with self._lock:
            grant = self._grants.get(grant_id)
            if not grant:
                raise AuthorityError("grant_not_found")
            grant.revoked = True
            result = asdict(grant)
        self._audit("revoked", result)
        return result

    def revoke_all(self) -> int:
        count = 0
        with self._lock:
            for grant in self._grants.values():
                if not grant.revoked and grant.expires_at > time.time():
                    grant.revoked = True
                    count += 1
        self._audit("revoke_all", {"count": count})
        return count

    # --------------------------------------------------------------- audit
    def audit_action(self, event: str, details: dict[str, Any]) -> None:
        self._audit(event, details)

    def recent_audit(self, limit: int = 50) -> list[dict[str, Any]]:
        if not self._audit_path.exists():
            return []
        try:
            lines = self._audit_path.read_text(encoding="utf-8").splitlines()
        except OSError:
            return []
        result: list[dict[str, Any]] = []
        for line in lines[-max(1, min(limit, 200)) :]:
            try:
                result.append(json.loads(line))
            except json.JSONDecodeError:
                continue
        return result

    def _audit(self, event: str, details: dict[str, Any]) -> None:
        safe = dict(details)
        safe.pop("grant_token", None)
        record = {"at": time.time(), "event": event, "details": safe}
        try:
            with self._audit_path.open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(record, separators=(",", ":"), default=str) + "\n")
        except OSError:
            # Authority still works if the audit disk is temporarily read-only;
            # callers can surface the missing audit separately rather than
            # silently authorising themselves.
            pass
