"""Conversation memory.

The whole point of MAZ Talk is that the second question can say "turn that into
a 25-minute goal" and be understood. The Cardputer holds only a session id; the
history lives here.
"""

from __future__ import annotations

from mazhost.sessions import SessionStore


def test_turns_accumulate_and_come_back_in_order():
    store = SessionStore(max_turns=10, ttl_minutes=60)
    sid = store.start()

    store.add_turn(sid, "I am trying to finish MAZ Pocket today.", "Noted.")
    store.add_turn(sid, "What should I do next?", "Finish the mic test.")

    messages = store.messages(sid)
    assert [m["role"] for m in messages] == ["user", "assistant", "user", "assistant"]
    assert messages[0]["content"] == "I am trying to finish MAZ Pocket today."
    assert messages[3]["content"] == "Finish the mic test."


def test_old_turns_are_trimmed_as_complete_pairs():
    store = SessionStore(max_turns=2, ttl_minutes=60)
    sid = store.start()
    for index in range(3):
        store.add_turn(sid, f"question {index}", f"answer {index}")
    assert [message["content"] for message in store.messages(sid)] == [
        "question 1", "answer 1", "question 2", "answer 2"
    ]
