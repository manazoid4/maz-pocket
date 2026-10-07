"""One-click setup contracts: short-code pairing returns the token LAN-only, private-repo update message,
LAN discovery responder, and the installer scripts staying ASCII / PS 5.1 friendly."""
from __future__ import annotations

import re
import socket
from pathlib import Path

import httpx

from mazhost.discovery import QUERY, DiscoveryResponder, build_reply
from mazhost.selfupdate import PRIVATE_REPO_MSG, SelfUpdater
from test_remote_guard import FUNNEL, TOKEN, _client

ROOT = Path(__file__).resolve().parents[2]


def test_claim_returns_token_on_lan_once_and_remote_is_refused(tmp_path):
    lan = _client(tmp_path, peer="192.168.1.50")
    # loopback minting, exactly what nod-setup.ps1 does
    local = _client(tmp_path)
    code = local.post("/pair/start", headers={"Authorization": f"Bearer {TOKEN}"}).json()["code"]
    assert len(code) == 8
    # a remote (Funnel) peer must never obtain the token, and a refused try does not burn the code
    assert _client(tmp_path, peer="127.0.0.1").post("/pair/claim", json={"code": code}, headers=FUNNEL).status_code == 403
    # separate app instances do not share state, so claim on the same instance that minted it
    ok = local.post("/pair/claim", json={"code": code})
    assert ok.status_code == 200 and ok.json()["token"] == TOKEN
    assert local.post("/pair/claim", json={"code": code}).status_code == 400
    # lan client reaches the claim route (no 403) even with a wrong code
    assert lan.post("/pair/claim", json={"code": "AAAAAAAA"}).status_code == 400


def test_health_is_minimal_without_token(tmp_path):
    body = _client(tmp_path, peer="192.168.1.50").get("/health").json()
    assert body == {"ok": True, "name": "nod Core", "version": body["version"]}


def _updater(status, token=""):
    http = httpx.Client(transport=httpx.MockTransport(lambda req: httpx.Response(status, json={"message": "x"})))
    return SelfUpdater(repo_dir="", http=http, install_requirements=False, github_token=token)


def test_private_repo_404_gives_one_clear_line():
    st = _updater(404).check()
    assert st["last_result"] == "error"
    assert st["last_error"] == PRIVATE_REPO_MSG == "repo is private - set MAZ_GITHUB_TOKEN or make it public"


def test_404_with_token_and_403_messages_do_not_claim_private():
    assert "no access" in _updater(404, token="t").check()["last_error"]
    assert "rate limit" in _updater(403).check()["last_error"]


def test_core_update_endpoint_shows_the_message(tmp_path):
    c = _client(tmp_path)
    st = c.get("/core/update", headers={"Authorization": f"Bearer {TOKEN}"})
    assert st.status_code == 200 and "last_error" in st.json()


def test_discovery_responder_answers_only_the_query():
    r = DiscoveryResponder(port=0, bind="127.0.0.1")
    assert r.start()
    try:
        r.port = r.bound_port
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(2)
        s.sendto(QUERY, ("127.0.0.1", r.bound_port))
        data, _ = s.recvfrom(256)
        assert data == build_reply(r.bound_port) and data.startswith(b"NOD-CORE {")
        s.sendto(b"hello", ("127.0.0.1", r.bound_port))
        s.settimeout(0.5)
        try:
            s.recvfrom(256)
            raise AssertionError("must not answer arbitrary datagrams")
        except socket.timeout:
            pass
    finally:
        r.stop()


def test_installer_is_ascii_ps51_safe_and_never_prints_the_token():
    for name in ("nod-setup.cmd", "nod-setup.ps1"):
        raw = (ROOT / "host" / name).read_bytes()
        raw.decode("ascii")  # ASCII only
    ps = (ROOT / "host/nod-setup.ps1").read_text(encoding="ascii")
    assert not re.search(r"\?\?|\?\.|\s\?\s.+\s:\s", ps.replace("-?", "")), "PS7-only syntax"
    for line in ps.splitlines():
        if re.search(r"Write-Host|Write-Output|Write-Warning", line):
            assert not re.search(r"\$(Token|MazToken)\b", line), line
    assert "/pair/start" in ps and "/core/update/check" in ps and "nod Core" in ps
