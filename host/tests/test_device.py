from types import SimpleNamespace

from mazhost.device import select_port


def test_select_port_follows_cardputer_usb_identity():
    ports = [
        SimpleNamespace(device="COM2", vid=0x1234, pid=0x0001),
        SimpleNamespace(device="COM5", vid=0x303A, pid=0x1001),
    ]
    assert select_port("", 0x303A, 0x1001, ports) == "COM5"


def test_configured_port_wins_without_scanning():
    assert select_port("COM9", 0x303A, 0x1001, []) == "COM9"
