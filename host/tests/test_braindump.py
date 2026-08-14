from mazhost.braindump import structure_braindump


def test_accepts_fenced_model_json():
    value, fallback = structure_braindump(
        '```json\n{"summary":"Ready","ideas":[],"actions":["ship"],"questions":[]}\n```',
        "ignored",
    )

    assert fallback is False
    assert value["summary"] == "Ready"
    assert value["actions"] == ["ship"]


def test_malformed_model_output_falls_back_to_transcript():
    value, fallback = structure_braindump(
        '```json\n{"summary":"broken","actions":["test"]}\nextra',
        "I decided to use MAZ Pocket. The action is to test every shortcut.",
    )

    assert fallback is True
    assert value["summary"].startswith("I decided to use MAZ Pocket")
    assert value["actions"] == ["test every shortcut"]


def test_empty_transcript_is_honest():
    value, fallback = structure_braindump("not json", "   ")

    assert fallback is True
    assert value == {
        "summary": "No clear speech was transcribed.",
        "ideas": [],
        "actions": [],
        "questions": [],
    }
