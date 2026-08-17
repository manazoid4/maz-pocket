from __future__ import annotations

import json
from pathlib import Path

from mazhost.beam import BeamStore, classify
from mazhost.config import Settings
from mazhost.llm import Models
from mazhost.telemetry import parse_nvidia_smi_line


def test_beam_store_survives_restart_and_pull_acknowledges(tmp_path: Path):
    path = tmp_path / "beam.json"
    first = BeamStore(path)
    message = first.to_pocket("https://example.com/festival")
    assert message["kind"] == "link"

    second = BeamStore(path)
    pulled = second.pull()
    assert pulled and pulled["text"] == "https://example.com/festival"
    assert second.pull() is None

    saved = json.loads(path.read_text(encoding="utf-8"))
    assert saved["to_pocket"] == []


def test_beam_classification_is_data_only():
    assert classify("https://example.com") == "link"
    assert classify("powershell Remove-Item C:\\*") == "text"


def test_nvidia_parser_is_small_and_defensive():
    assert parse_nvidia_smi_line("42, 3072, 6144, 58") == {
        "util_pct": 42,
        "vram_used_mb": 3072,
        "vram_total_mb": 6144,
        "temp_c": 58,
    }
    assert parse_nvidia_smi_line("not,a,gpu") is None


def test_smart_profile_uses_small_context_until_needed():
    cfg = Settings(
        token="test-token-that-is-not-default",
        ai_profile="smart",
        ollama_model="qwen:test",
        _env_file=None,
    )
    models = Models(cfg)
    assert models._context_size([{"role": "user", "content": "hello"}]) == 4096
    assert models._context_size([{"role": "user", "content": "x" * 12_000}]) == 6144
    assert models._context_size([{"role": "user", "content": "x" * 22_000}]) == 8192
    assert models._keep_alive() == "15m"


def test_save_and_fast_profiles_only_change_resource_policy():
    save = Models(Settings(token="test-token-that-is-not-default", ai_profile="save", _env_file=None))
    fast = Models(Settings(token="test-token-that-is-not-default", ai_profile="fast", _env_file=None))
    assert save._keep_alive() == 0
    assert fast._keep_alive() == "60m"
