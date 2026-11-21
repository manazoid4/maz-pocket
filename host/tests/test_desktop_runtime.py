from __future__ import annotations

from pathlib import Path

from mazhost.desktop_runtime import CoreRuntime, ensure_config, read_env


def test_first_run_generates_credential_and_preserves_it(tmp_path: Path):
    path = tmp_path / ".env"
    first = ensure_config(path)
    second = ensure_config(path)
    assert len(first["MAZ_TOKEN"]) >= 32
    assert second["MAZ_TOKEN"] == first["MAZ_TOKEN"]
    assert second["MAZ_DEFAULT_ROUTE"] == "auto"


def test_existing_settings_are_preserved(tmp_path: Path):
    path = tmp_path / ".env"
    path.write_text("MAZ_TOKEN=existing-secret\nMAZ_DEFAULT_ROUTE=cloud\nCUSTOM=keep\n", encoding="utf-8")
    ensure_config(path)
    values = read_env(path)
    assert values["MAZ_TOKEN"] == "existing-secret"
    assert values["MAZ_DEFAULT_ROUTE"] == "cloud"
    assert values["CUSTOM"] == "keep"


def test_restart_refuses_to_kill_unowned_core(monkeypatch, tmp_path: Path):
    runtime = CoreRuntime(tmp_path)
    monkeypatch.setattr(runtime, "health", lambda: {"ok": True})
    try:
        runtime.restart()
    except RuntimeError as error:
        assert "outside this window" in str(error)
    else:
        raise AssertionError("unowned Core restart was allowed")
