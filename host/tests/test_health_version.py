from mazhost.version import CORE_VERSION
from test_app import client


def test_health_reports_version_and_name():
    body = client().get("/health", headers={"Authorization": "Bearer test-token-that-is-not-default"}).json()
    assert body["version"] == CORE_VERSION
    assert body["name"] == "nod Core"
