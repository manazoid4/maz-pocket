

def test_prompt_contains_today_and_no_time_refusal():
    from datetime import datetime
    from zoneinfo import ZoneInfo
    import test_app
    models = test_app.FakeModels()
    api = test_app.client(models=models)
    h = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=h).json()["session_id"]
    api.post("/turn/text", headers=h, json={"text": "what time is it", "session_id": sid, "route": "auto"})
    system = models.last_messages[0]["content"]
    now = datetime.now(ZoneInfo("Europe/London"))
    assert f"{now.day} {now:%B %Y}" in system and "Location: UK" in system
    assert "never say you lack access" in system
