from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "protocol" / "action_ids.json"
CPP = ROOT / "src" / "net" / "action_ids.generated.h"
PY = ROOT / "host" / "mazhost" / "action_ids.py"


def const_name(prefix: str, value: str) -> str:
    return prefix + "_" + re.sub(r"[^A-Za-z0-9]+", "_", value).upper()


def render_cpp(spec: dict[str, list[str]]) -> str:
    pc = spec["pc"]
    core = spec["core"]
    lines = [
        "// Generated from protocol/action_ids.json by scripts/gen_action_ids.py.",
        "// Do not edit by hand.",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "",
        "namespace maz {",
        "namespace action_ids {",
        "",
    ]
    for value in pc:
        lines.append(f'constexpr const char* {const_name("PC", value)} = "{value}";')
    lines += ["", "constexpr const char* PC[] = {"]
    names = [const_name("PC", value) for value in pc]
    for i in range(0, len(names), 4):
        lines.append("    " + ", ".join(names[i : i + 4]) + ",")
    lines += [
        "};",
        "constexpr size_t PC_COUNT = sizeof(PC) / sizeof(PC[0]);",
        "",
    ]
    for value in core:
        lines.append(f'constexpr const char* {const_name("CORE", value)} = "{value}";')
    lines += ["", "constexpr const char* CORE[] = {"]
    names = [const_name("CORE", value) for value in core]
    for i in range(0, len(names), 3):
        lines.append("    " + ", ".join(names[i : i + 3]) + ",")
    lines += [
        "};",
        "constexpr size_t CORE_COUNT = sizeof(CORE) / sizeof(CORE[0]);",
        "",
        "}  // namespace action_ids",
        "}  // namespace maz",
        "",
    ]
    return "\n".join(lines)


def render_py(spec: dict[str, list[str]]) -> str:
    lines = [
        '"""Generated from protocol/action_ids.json by scripts/gen_action_ids.py."""',
        "",
        "PC_ACTIONS = (",
    ]
    lines += [f'    "{value}",' for value in spec["pc"]]
    lines += [")", "", "CORE_ACTIONS = ("]
    lines += [f'    "{value}",' for value in spec["core"]]
    lines += [
        ")",
        "",
        "PC_ACTION_SET = frozenset(PC_ACTIONS)",
        "CORE_ACTION_SET = frozenset(CORE_ACTIONS)",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    spec = json.loads(SPEC.read_text(encoding="utf-8"))
    if sorted(spec) != ["core", "pc"]:
        raise SystemExit("action_ids.json must contain exactly core + pc")
    for group, values in spec.items():
        if not isinstance(values, list) or not values or len(values) != len(set(values)):
            raise SystemExit(f"{group}: IDs must be a unique non-empty list")
        for value in values:
            if not re.fullmatch(r"[a-z][a-z0-9_]{1,63}", value):
                raise SystemExit(f"invalid action ID: {value!r}")

    expected = {CPP: render_cpp(spec), PY: render_py(spec)}
    if args.check:
        bad = [str(path.relative_to(ROOT)) for path, text in expected.items()
               if not path.exists() or path.read_text(encoding="utf-8") != text]
        if bad:
            raise SystemExit("generated action IDs are stale: " + ", ".join(bad))
        print("action ID generated files: PASS")
        return 0

    for path, text in expected.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    print("generated action IDs updated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
