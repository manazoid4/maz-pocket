from mazhost.commands import parse_command


def test_parses_relative_reminder_without_an_llm():
    assert parse_command("remind me in 30 minutes to check the chicken") == {
        "type": "reminder.create",
        "delay_seconds": 1800,
        "title": "check the chicken",
    }


def test_parses_small_pc_control_allowlist_without_an_llm():
    assert parse_command("mute my pc") == {"type": "pc.action", "action": "mute"}
    assert parse_command("show desktop") == {"type": "pc.action", "action": "desktop"}
    assert parse_command("next track") == {"type": "pc.action", "action": "next_track"}
    assert parse_command("lock my computer") == {"type": "pc.action", "action": "lock"}


def test_pc_words_inside_a_question_do_not_execute_actions():
    assert parse_command("why is my pc muted?") is None
    assert parse_command("should I lock my computer when I leave?") is None


def test_rejects_non_commands():
    assert parse_command("what should I do next?") is None
