from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_route_persistence_prefers_symbolic_identity_with_legacy_byte_compatibility():
    header = (ROOT / "src/core/settings.h").read_text(encoding="utf-8")
    implementation = (ROOT / "src/core/settings.cpp").read_text(encoding="utf-8")

    assert 'route == "mazlatest"' in header
    assert 'prefs.getString("route_id"' in implementation
    assert 'prefs.putString("route_id", talkRouteApiName(talkRoute))' in implementation
    assert 'prefs.getUChar("route"' in implementation
    assert 'prefs.putUChar("route", talkRoute)' in implementation


def test_current_portals_send_and_report_symbolic_route_names():
    for relative_path in ("src/net/portal_v3.cpp", "src/net/web.cpp"):
        source = (ROOT / relative_path).read_text(encoding="utf-8")
        assert '"route_id"' in source or '\\"route_id\\"' in source
        assert 'value="mazlatest"' in source or 'value=mazlatest' in source
        assert "talkRouteFromApiName" in source
