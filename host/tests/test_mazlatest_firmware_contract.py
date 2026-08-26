"""Static guard for the Cardputer's explicit fourth AI route."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_cardputer_route_cycle_and_host_payload_are_centrally_mapped():
    settings = (ROOT / "src/core/settings.h").read_text(encoding="utf-8-sig")
    assert "TALK_ROUTE_COUNT = 4" in settings
    assert 'case 3: return "mazlatest"' in settings
    assert 'case 3: return "MAZLATEST"' in settings

    for relative in (
        "src/apps/comm.cpp",
        "src/apps/v03.cpp",
        "src/apps/voice_apps.cpp",
    ):
        source = (ROOT / relative).read_text(encoding="utf-8-sig")
        assert "% TALK_ROUTE_COUNT" in source
        assert "% 3" not in source

    for relative in (
        "src/net/mazhost.cpp",
        "src/net/field_host.cpp",
        "src/net/workflow_client.cpp",
    ):
        source = (ROOT / relative).read_text(encoding="utf-8-sig")
        assert "talkRouteApiName(Cfg.talkRoute)" in source


def test_all_firmware_configuration_surfaces_accept_route_three():
    for relative in (
        "src/net/portal.cpp",
        "src/net/portal_v2.cpp",
        "src/net/portal_v3.cpp",
        "src/net/web.cpp",
    ):
        source = (ROOT / relative).read_text(encoding="utf-8-sig")
        assert "MAZLATEST" in source
        assert "<= 2" not in source
        assert "parsedRoute > 2" not in source


def test_installer_pins_the_explicit_loopback_9router_route():
    installer = (ROOT / "host/install-core.ps1").read_text(encoding="utf-8-sig")
    setup = (ROOT / "host/setup.ps1").read_text(encoding="utf-8-sig")
    assert 'Set-MazEnv "MAZ_MAZLATEST_URL" "http://localhost:20128/v1"' in installer
    assert 'Set-MazEnv "MAZ_MAZLATEST_MODEL" "MazLatest"' in installer
    assert 'Write-Host "Pair token: $Token"' not in installer
    assert 'Write-Host "  Token:   $ConfiguredToken"' not in setup
    assert "secret not printed" in installer
