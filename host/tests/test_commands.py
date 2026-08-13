from mazhost.commands import parse_command


def test_parses_relative_reminder_without_an_llm():
    assert parse_command("remind me in 30 minutes to check the chicken") == {
        "type": "reminder.create",
        "delay_seconds": 1800,
        "title": "check the chicken",
    }


def test_rejects_non_commands():
    assert parse_command("what should I do next?") is None
