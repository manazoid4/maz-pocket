from __future__ import annotations

import httpx
import pytest

from mazhost.config import Settings
from mazhost.errors import ErrorCode, RouteError
from mazhost.llm import Models


def models_with(handler) -> Models:
    settings = Settings(
        token="test-token-that-is-not-default",
        cloud_url="http://provider.example/v1",
        cloud_key="example-key",
        cloud_model="example/model",
        _env_file=None,
    )
    return Models(settings, client=httpx.Client(transport=httpx.MockTransport(handler)))


@pytest.mark.parametrize(
    ("status", "body", "code", "retryable"),
    [
        (401, {"error": {"message": "unauthorized"}}, ErrorCode.AUTH_FAILED, False),
        (404, {"error": {"message": "model example/model not found"}}, ErrorCode.MODEL_NOT_FOUND, False),
        (429, {"error": {"message": "rate limited"}}, ErrorCode.RATE_LIMITED, True),
        (500, {"error": {"message": "failed"}}, ErrorCode.UPSTREAM_UNAVAILABLE, True),
    ],
)
def test_cloud_http_failures_are_typed(status, body, code, retryable):
    def handler(_request):
        return httpx.Response(status, json=body)

    with pytest.raises(RouteError) as raised:
        models_with(handler).chat([{"role": "user", "content": "hello"}], "cloud")
    assert raised.value.code == code
    assert raised.value.retryable is retryable
    assert raised.value.payload()["route"] == "cloud"


def test_cloud_timeout_is_typed_and_retryable():
    def handler(request):
        raise httpx.ReadTimeout("slow", request=request)

    with pytest.raises(RouteError) as raised:
        models_with(handler).chat([{"role": "user", "content": "hello"}], "cloud")
    assert raised.value.code == ErrorCode.UPSTREAM_TIMEOUT
    assert raised.value.retryable is True


def test_malformed_success_is_not_accepted():
    def handler(_request):
        return httpx.Response(200, json={"choices": []})

    with pytest.raises(RouteError) as raised:
        models_with(handler).chat([{"role": "user", "content": "hello"}], "cloud")
    assert raised.value.code == ErrorCode.INVALID_UPSTREAM_RESPONSE

