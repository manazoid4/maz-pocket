"""Minimal ArduinoOTA client for the ESP32 Arduino 2.x protocol used by v0.3.

This is intentionally small and independent of PlatformIO. The Windows updater
can therefore upload the same application binary over Wi-Fi without asking the
user to install a toolchain.
"""

from __future__ import annotations

import hashlib
import random
import socket
from pathlib import Path
from typing import Callable


Progress = Callable[[float, str], None]


def _local_ip_for(remote_ip: str) -> str:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect((remote_ip, 3232))
        return probe.getsockname()[0]
    finally:
        probe.close()


def upload(ip: str, password: str, firmware: Path, progress: Progress | None = None) -> None:
    firmware = Path(firmware)
    if not firmware.exists():
        raise RuntimeError(f"Firmware not found: {firmware}")
    size = firmware.stat().st_size
    digest = hashlib.md5(firmware.read_bytes()).hexdigest()
    local_ip = _local_ip_for(ip)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    local_port = random.randint(10000, 60000)
    server.bind((local_ip, local_port))
    server.listen(1)
    server.settimeout(12)

    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.settimeout(2)
    remote = (ip, 3232)
    invitation = f"0 {local_port} {size} {digest}\n".encode()

    try:
        reply = ""
        for _ in range(10):
            udp.sendto(invitation, remote)
            try:
                reply = udp.recv(96).decode(errors="replace").strip()
                break
            except socket.timeout:
                continue
        if not reply:
            raise RuntimeError("Cardputer did not answer the OTA invitation")

        if reply.startswith("AUTH "):
            if not password:
                raise RuntimeError("OTA password/token is required")
            nonce = reply.split(maxsplit=1)[1]
            # Arduino ESP32 2.x derives its client nonce from these four values.
            cnonce_text = f"{firmware}{size}{digest}{ip}"
            cnonce = hashlib.md5(cnonce_text.encode()).hexdigest()
            pass_md5 = hashlib.md5(password.encode()).hexdigest()
            response = hashlib.md5(f"{pass_md5}:{nonce}:{cnonce}".encode()).hexdigest()
            udp.sendto(f"200 {cnonce} {response}\n".encode(), remote)
            auth = udp.recv(96).decode(errors="replace").strip()
            if auth != "OK":
                raise RuntimeError(f"OTA authentication failed: {auth}")
        elif reply != "OK":
            raise RuntimeError(f"Unexpected OTA reply: {reply}")

        if progress:
            progress(0.02, "Cardputer accepted update")
        connection, _ = server.accept()
        connection.settimeout(15)
        try:
            sent = 0
            last_ack = ""
            with firmware.open("rb") as source:
                while True:
                    chunk = source.read(1024)
                    if not chunk:
                        break
                    connection.sendall(chunk)
                    last_ack = connection.recv(32).decode(errors="replace").strip()
                    if not last_ack:
                        raise RuntimeError("Cardputer stopped acknowledging OTA writes")
                    sent += len(chunk)
                    if progress:
                        progress(sent / max(1, size), f"Wi-Fi update {sent * 100 // max(1, size)}%")

            # During transfer ArduinoOTA may acknowledge with byte counts; the
            # success marker is OK either on the final chunk or in the result
            # frame immediately following it.
            if "OK" not in last_ack:
                connection.settimeout(12)
                final = ""
                for _ in range(5):
                    try:
                        final = connection.recv(64).decode(errors="replace").strip()
                    except socket.timeout:
                        continue
                    if "OK" in final:
                        break
                if "OK" not in final:
                    raise RuntimeError(f"OTA did not report success: {final or last_ack}")
        finally:
            connection.close()
    finally:
        udp.close()
        server.close()

    if progress:
        progress(1.0, "Wi-Fi update complete; Cardputer is rebooting")
