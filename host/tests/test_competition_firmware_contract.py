from pathlib import Path


ROOT = Path(__file__).parents[2]


def test_home_exposes_only_four_competition_surfaces():
    registry = (ROOT / "src/apps/registry.cpp").read_text(encoding="utf-8")
    home_rows = [line for line in registry.splitlines() if "true," in line and '{"' in line]
    assert [line.split('"')[1] for line in home_rows] == [
        "talk",
        "capturehub",
        "agents",
        "desk",
    ]


def test_plan_does_not_fetch_projects_synchronously_on_entry():
    source = (ROOT / "src/apps/agent_tools.cpp").read_text(encoding="utf-8")
    enter = source.split("void onEnter() override {", 1)[1].split("bool onKey", 1)[0]
    assert "_kind == ToolKind::Plan ? View::Task" in enter
    assert "if (_view != View::Task) refreshProjects();" in enter
