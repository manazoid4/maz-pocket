from __future__ import annotations

import json
from pathlib import Path

from mazhost.action_ids import CORE_ACTIONS, PC_ACTIONS
from mazhost.pc import PCController

ROOT = Path(__file__).resolve().parents[2]


def test_generated_action_ids_match_protocol_spec():
    spec = json.loads((ROOT / "protocol" / "action_ids.json").read_text(encoding="utf-8"))
    assert tuple(spec["pc"]) == PC_ACTIONS
    assert tuple(spec["core"]) == CORE_ACTIONS
    assert set(PCController.LABELS) == set(PC_ACTIONS)


def test_core_and_api_do_not_invent_action_ids():
    """Temporary textual gate until the large Core/API files are regenerated.

    Execution remains independently allow-listed, but this makes any drift from
    the one canonical protocol file fail CI instead of silently creating a new
    Cardputer/Core command vocabulary.
    """
    spec = json.loads((ROOT / "protocol" / "action_ids.json").read_text(encoding="utf-8"))
    core_source = (ROOT / "host" / "mazhost" / "core.py").read_text(encoding="utf-8")
    app_source = (ROOT / "host" / "mazhost" / "app.py").read_text(encoding="utf-8")
    for action in spec["core"]:
        assert f'"{action}"' in core_source
        assert f'"{action}"' in app_source
    for action in spec["pc"]:
        assert f'"{action}"' in app_source
