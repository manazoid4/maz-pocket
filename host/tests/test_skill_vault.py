from __future__ import annotations

import json

from mazhost.config import Settings
from mazhost.skill_vault import SkillVault


class FakeModels:
    def chat(self, messages, route):
        return json.dumps({
            "name": "Audit project before build",
            "description": "Inspect project state and validate before building.",
            "when_to_use": ["Before a consequential project build."],
            "procedure": ["Read project instructions.", "Check git status.", "Run the relevant tests."],
            "verification": ["Tests pass and the intended project state is clear."],
            "safety": ["Do not discard unrelated working-tree changes."],
        }), "fake"


def test_teach_skill_is_draft_until_explicit_approval(tmp_path):
    teach = tmp_path / "teach" / "teach_abcdefghi"
    teach.mkdir(parents=True)
    (teach / "manifest.json").write_text(json.dumps({
        "session_id": "teach_abcdefghi",
        "transcript": "First read the project instructions. Then check git status. Finally run tests.",
    }), encoding="utf-8")
    (teach / "timeline.jsonl").write_text(json.dumps({"t": 10, "type": "mark", "note": "tests"}) + "\n", encoding="utf-8")
    (teach / "transcript.json").write_text(json.dumps({"text": "Read instructions, check git status, run tests."}), encoding="utf-8")

    cfg = Settings(
        _env_file=None,
        token="configured-token",
        teach_dir=str(tmp_path / "teach"),
        skill_vault_dir=str(tmp_path / "skills"),
        skill_draft_dir=str(tmp_path / "drafts"),
    )
    vault = SkillVault(cfg, FakeModels())
    draft = vault.draft_from_teach("teach_abcdefghi", "local")
    assert draft["state"] == "draft"
    assert vault.list_skills() == []
    assert "## Procedure" in draft["skill_md"]

    approved = vault.approve(draft["draft_id"])
    assert approved["state"] == "approved"
    skills = vault.list_skills()
    assert len(skills) == 1
    assert skills[0]["slug"] == "audit-project-before-build"
