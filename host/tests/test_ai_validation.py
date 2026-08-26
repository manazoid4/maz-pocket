from __future__ import annotations

from mazhost.ai_validation import normalize_answer, run_semantic_suite


def test_normalize_answer_allows_safe_formatting_only():
    assert normalize_answer(" **Paris.** ") == "paris"
    assert normalize_answer("`4`") == "4"


def test_http_success_with_wrong_content_fails_semantic_validation():
    class WrongModel:
        _last_route = {"active": "mazlatest"}
        _last_usage = {"model": "wrong-model"}

        def chat(self, _messages, _route):
            return "5", "mazlatest:wrong-model"

    results = run_semantic_suite(WrongModel(), "mazlatest")

    assert results[0]["expected"] == "4"
    assert results[0]["normalized"] == "5"
    assert results[0]["pass"] is False
    assert not all(result["pass"] for result in results)
