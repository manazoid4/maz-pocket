from __future__ import annotations

import time

import serial
from serial.tools import list_ports

from .desktop_runtime import lan_address


def pair(values: dict[str, str], port: str = "", address: str = "") -> dict:
    candidates = [item for item in list_ports.comports() if item.vid == 0x303A and item.pid == 0x1001]
    if port:
        candidates = [item for item in candidates if item.device.upper() == port.upper()]
    if len(candidates) != 1:
        raise RuntimeError("Connect exactly one Cardputer ADV by USB")
    token = values.get("MAZ_TOKEN", "")
    if not token:
        raise RuntimeError("MAZ Core credential is missing")
    target = address or lan_address()
    core_port = int(values.get("MAZ_PORT", "8787"))
    device = serial.Serial(port=None, baudrate=115200, timeout=0.25)
    device.dtr = False
    device.rts = False
    device.port = candidates[0].device
    with device:
        # Opening native USB may restart firmware. Wait for the application to
        # finish booting, then discard banners without ever logging the token.
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            line = device.readline().decode(errors="replace").strip()
            if "READY" in line:
                break
        device.reset_input_buffer()
        device.write(f"MAZCOREPAIR\t{target}\t{core_port}\t{token}\n".encode())
        device.flush()
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            reply = device.readline().decode(errors="replace").strip()
            if reply.startswith("MAZCOREPAIR OK"):
                return {"ok": True, "port": candidates[0].device, "address": target, "core_port": core_port}
            if reply.startswith("MAZCOREPAIR ERR"):
                raise RuntimeError("Cardputer rejected MAZ Core pairing")
    raise RuntimeError("Cardputer did not confirm pairing; ensure MAZ Pocket is open")
