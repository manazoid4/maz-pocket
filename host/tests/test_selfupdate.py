import hashlib
import json
import subprocess

import httpx
import pytest

from mazhost.selfupdate import SelfUpdater, pick_release
from test_app import client

AUTH = {"Authorization": "Bearer test-token-that-is-not-default"}
FW = b"firmware-bytes" * 10


def git(cwd, *args):
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True,
                          env={"GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@t", "GIT_COMMITTER_NAME": "t",
                               "GIT_COMMITTER_EMAIL": "t@t", "PATH": __import__("os").environ["PATH"],
                               "HOME": str(cwd)}).stdout.strip()


@pytest.fixture
def repos(tmp_path):
    origin = tmp_path / "origin.git"
    work = tmp_path / "work"
    git(tmp_path, "init", "--bare", "-b", "deploy/local", str(origin))
    git(tmp_path, "clone", str(origin), str(work))
    git(work, "checkout", "-B", "deploy/local")
    (work / "a.txt").write_text("1")
    git(work, "add", "."); git(work, "commit", "-m", "one"); git(work, "push", "origin", "deploy/local")
    first = git(work, "rev-parse", "HEAD")
    (work / "a.txt").write_text("2")
    git(work, "commit", "-am", "two"); git(work, "push", "origin", "deploy/local")
    second = git(work, "rev-parse", "HEAD")
    git(work, "checkout", "--detach", first)
    return work, first, second


def release(tag, git_sha, fw=FW, draft=False, published="2026-01-01T00:00:00Z", sha256=None):
    m = {"version": "1.2.3", "sha": "abc1234", "sha256": sha256 or hashlib.sha256(fw).hexdigest(),
         "size": len(fw), "build": 5, "git_sha": git_sha, "core_version": "0.9", "tag": tag}
    return {"tag_name": tag, "draft": draft, "published_at": published, "assets": [
        {"name": "nod-fw.bin", "browser_download_url": f"https://dl/{tag}/nod-fw.bin"},
        {"name": "nod-manifest.json", "browser_download_url": f"https://dl/{tag}/nod-manifest.json"}]}, m, fw


def http_for(*rels):
    by_tag = {r[0]["tag_name"]: r for r in rels}

    def handler(req: httpx.Request) -> httpx.Response:
        url = str(req.url)
        if "api.github.com" in url:
            return httpx.Response(200, json=[r[0] for r in rels])
        tag = url.split("/")[3]
        _, m, fwb = by_tag[tag]
        return httpx.Response(200, json=m) if url.endswith(".json") else httpx.Response(200, content=fwb)
    return httpx.Client(transport=httpx.MockTransport(handler))


def updater(repo, http, restarts=None, **kw):
    return SelfUpdater(repo_dir=str(repo), http=http, install_requirements=False,
                       restart=lambda n, p: (restarts if restarts is not None else []).append((n, p)), **kw)


def test_picks_newest_nod_release():
    rels = [{"tag_name": "v9", "published_at": "2026-05-01"},
            {"tag_name": "nod-v1.0-b1", "published_at": "2026-01-01"},
            {"tag_name": "nod-v1.0-b3", "published_at": "2026-03-01"},
            {"tag_name": "nod-v1.0-b4", "published_at": "2026-04-01", "draft": True}]
    assert pick_release(rels)["tag_name"] == "nod-v1.0-b3"
    assert pick_release([{"tag_name": "x"}]) is None


def test_stages_fw_only_on_sha256_change(repos, tmp_path, monkeypatch):
    work, first, _ = repos
    fwdir = tmp_path / "fw"
    monkeypatch.setenv("MAZ_FW_DIR", str(fwdir))
    rel = release("nod-v1.2.3-b5", first)
    u = updater(work, http_for(rel))
    assert "fw_staged" in u.check()["last_result"]
    assert (fwdir / "latest.bin").read_bytes() == FW
    man = json.loads((fwdir / "manifest.json").read_text())
    assert man["sha256"] == rel[1]["sha256"] and man["version"] == "1.2.3" and man["size"] == len(FW)
    mtime = (fwdir / "latest.bin").stat().st_mtime_ns
    assert "fw_current" in u.check()["last_result"]
    assert (fwdir / "latest.bin").stat().st_mtime_ns == mtime
    assert not list(fwdir.glob("*.part"))


def test_rejects_sha256_mismatch(repos, tmp_path, monkeypatch):
    work, first, _ = repos
    fwdir = tmp_path / "fw"
    monkeypatch.setenv("MAZ_FW_DIR", str(fwdir))
    st = updater(work, http_for(release("nod-v1.2.3-b5", first, sha256="0" * 64))).check()
    assert st["last_result"] == "error" and "fw_verify_failed" in st["last_error"]
    assert not (fwdir / "latest.bin").exists() and not (fwdir / "manifest.json").exists()
    assert not list(fwdir.glob("*.part"))


def test_updates_code_when_clean_and_on_branch(repos, tmp_path, monkeypatch):
    work, first, second = repos
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path / "fw"))
    (work / "host").mkdir(); (work / "host" / "requirements.txt").write_text("")
    restarts = []
    u = updater(work, http_for(release("nod-v1.2.3-b5", second)), restarts)
    assert "code_updated_restarting" in u.check()["last_result"]
    assert git(work, "rev-parse", "HEAD") == second
    assert restarts == [(second, first)]


def test_skips_code_update_when_dirty(repos, tmp_path, monkeypatch):
    work, first, second = repos
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path / "fw"))
    (work / "a.txt").write_text("local edit")
    restarts = []
    st = updater(work, http_for(release("nod-v1.2.3-b5", second)), restarts).check()
    assert "code_skipped_dirty" in st["last_result"]
    assert git(work, "rev-parse", "HEAD") == first and not restarts


def test_untracked_files_do_not_block(repos, tmp_path, monkeypatch):
    work, first, second = repos
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path / "fw"))
    (work / "scratch.log").write_text("x")
    (work / "host").mkdir(); (work / "host" / "requirements.txt").write_text("")
    assert "code_updated_restarting" in updater(work, http_for(release("nod-v1.2.3-b5", second))).check()["last_result"]


def test_skips_when_git_sha_not_on_origin_branch(repos, tmp_path, monkeypatch):
    work, first, _ = repos
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path / "fw"))
    (work / "b.txt").write_text("rogue")
    git(work, "add", "."); git(work, "commit", "-m", "unpushed")
    rogue = git(work, "rev-parse", "HEAD")
    git(work, "checkout", "--detach", first)
    restarts = []
    st = updater(work, http_for(release("nod-v1.2.3-b5", rogue)), restarts).check()
    assert "code_skipped_not_on_branch" in st["last_result"]
    assert git(work, "rev-parse", "HEAD") == first and not restarts


def test_defers_when_busy(repos, tmp_path, monkeypatch):
    work, first, second = repos
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path / "fw"))
    st = updater(work, http_for(release("nod-v1.2.3-b5", second)), busy=lambda: True).check()
    assert "code_deferred_busy" in st["last_result"]


def test_status_endpoints_need_auth():
    api = client()
    assert api.get("/core/update").status_code == 401
    assert api.post("/core/update/check").status_code == 401
    body = api.get("/core/update", headers=AUTH).json()
    assert "running_git_sha" in body and "last_result" in body and "staged_fw" in body
    health = api.get("/health", headers=AUTH).json()
    assert "git_sha" in health and "update" in health


def test_restart_helper_task_quotes_paths_with_spaces():
    import subprocess as sp
    from pathlib import Path
    from xml.etree import ElementTree as ET
    from mazhost.selfupdate import restart_helper_args, restart_task_xml
    argv = restart_helper_args(Path("C:/My Repo/x"), "C:/MAZ Core/py.exe", "n" * 40, "p" * 40, 8787,
                               Path("C:/MAZ Core/.env"), Path("C:/MAZ Core/logs"))
    assert argv[0] == "powershell.exe" and argv[argv.index("-NewSha") + 1] == "n" * 40
    root = ET.fromstring(restart_task_xml(argv).encode("utf-16"))
    ns = {"t": "http://schemas.microsoft.com/windows/2004/02/mit/task"}
    assert root.find(".//t:Command", ns).text == "powershell.exe"
    args = root.find(".//t:Arguments", ns).text
    assert args == sp.list2cmdline(argv[1:])
    assert '"C:/MAZ Core/py.exe"' in args and 'logs"' in args



def test_fw_report_is_logged_and_readable(tmp_path, monkeypatch, caplog):
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path))
    api = client()
    body = {"stage": "begin", "error": "Boot not confirmed yet", "code": 0x1502, "slot": "nodfw1",
            "next_slot": "nodfw0", "size": 1458928, "build": "1e55d34", "ota_state": "pending_verify",
            "otadata": True, "battery": 13}
    assert api.post("/fw/report", json=body).status_code in (401, 403)  # no token
    with caplog.at_level("WARNING", logger="mazhost.fw"):
        r = api.post("/fw/report", json=body, headers=AUTH)
    assert r.status_code == 200
    assert "stage=begin" in caplog.text and "slot=nodfw1" in caplog.text
    last = api.get("/core/update", headers=AUTH).json()["fw_report"]
    assert last["stage"] == "begin" and last["code"] == 0x1502 and last["next_slot"] == "nodfw0" and last["at"]
    assert api.post("/fw/report", json={"error": "x"}, headers=AUTH).status_code == 422  # stage required
