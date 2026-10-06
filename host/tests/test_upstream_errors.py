from __future__ import annotations

import httpx
import pytest

from mazhost.config import Settings
from mazhost.errors import ErrorCode, RouteError
from mazhost.llm import Models


def settings() -> Settings:
    return Settings(
        token="test-token-that-is-not-default",
        cloud_url="http://9router.test/v1",
        cloud_key="test-key",
        cloud_model="test/model",
        _env_file=None,
    )


def model_with(handler) -> Models:
    return Models(settings(), client=httpx.Client(transport=httpx.MockTransport(handler)))


@pytest.mark.parametrize(
    ("status", "body", "code", "retryable"),
    [
        (401, {"error": {"message": "unauthorized"}}, ErrorCode.AUTH_FAILED, False),
        (404, {"error": {"message": "model test/model not found"}}, ErrorCode.MODEL_NOT_FOUND, False),
        (404, {"error": {"message": "not found"}}, ErrorCode.ENDPOINT_NOT_FOUND, False),
        (429, {"error": {"message": "rate limited"}}, ErrorCode.RATE_LIMITED, True),
        (500, {"error": {"message": "upstream failed"}}, ErrorCode.UPSTREAM_UNAVAILABLE, True),
    ],
)
def test_upstream_http_errors_are_normalized(status, body, code, retryable):
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(status, json=body)

    with pytest.raises(RouteError) as raised:
        model_with(handler).chat([{"role": "user", "content": "hello"}], "cloud")

    assert raised.value.code == code
    assert raised.value.upstream_status == status
    assert raised.value.retryable is retryable
    assert raised.value.payload()["route"] == "cloud"


@pytest.mark.parametrize(
    ("exception_factory", "code"),
    [
        (lambda request: httpx.ReadTimeout("slow", request=request), ErrorCode.UPSTREAM_TIMEOUT),
        (lambda request: httpx.ConnectError("refused", request=request), ErrorCode.UPSTREAM_UNAVAILABLE),
    ],
)
def test_upstream_transport_errors_are_normalized(exception_factory, code):
    def handler(request: httpx.Request) -> httpx.Response:
        raise exception_factory(request)

    with pytest.raises(RouteError) as raised:
        model_with(handler).chat([{"role": "user", "content": "hello"}], "cloud")

    assert raised.value.code == code
    assert raised.value.retryable is True


def test_malformed_success_is_not_treated_as_ai_success():
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(200, json={"choices": []})

    with pytest.raises(RouteError) as raised:
        model_with(handler).chat([{"role": "user", "content": "hello"}], "cloud")

    assert raised.value.code == ErrorCode.INVALID_UPSTREAM_RESPONSE


def test_local_refused_connection_is_distinct():
    def handler(request: httpx.Request) -> httpx.Response:
        raise httpx.ConnectError("refused", request=request)

    models = Models(
        Settings(
            token="test-token-that-is-not-default",
            local_engine="llamacpp",
            llamacpp_model="local-test",
            _env_file=None,
        ),
        client=httpx.Client(transport=httpx.MockTransport(handler)),
    )

    with pytest.raises(RouteError) as raised:
        models.chat([{"role": "user", "content": "hello"}], "local")

    assert raised.value.code == ErrorCode.LOCAL_UNAVAILABLE
    assert raised.value.route == "local"
