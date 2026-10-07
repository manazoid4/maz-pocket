"""LAN discovery so the Cardputer can find Core without anyone typing an IP.

The device broadcasts the UDP datagram b"NOD-CORE?" to port 8787 on its subnet. Core answers the sender
with b'NOD-CORE {"port": 8787, "name": "nod Core"}'. The reply carries no secret and no inventory; the
device then confirms with the unauthenticated minimal GET /health and pairs with a code via /pair/claim.
"""
from __future__ import annotations

import json
import logging
import socket
import threading

log = logging.getLogger("uvicorn.error.discovery")

QUERY = b"NOD-CORE?"


def build_reply(port: int) -> bytes:
    return b"NOD-CORE " + json.dumps({"port": port, "name": "nod Core"}).encode("ascii")


class DiscoveryResponder:
    def __init__(self, port: int = 8787, bind: str = "0.0.0.0") -> None:
        self.port = port
        self.bind = bind
        self.bound_port = 0
        self._sock: socket.socket | None = None
        self._thread: threading.Thread | None = None

    def start(self) -> bool:
        if self._thread:
            return True
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind((self.bind, self.port))
            s.settimeout(1.0)
        except OSError as e:  # port taken / no network: discovery is a convenience, never fatal
            log.warning("discovery: UDP %s unavailable (%s)", self.port, type(e).__name__)
            return False
        self._sock = s
        self.bound_port = s.getsockname()[1]
        self._thread = threading.Thread(target=self._run, name="nod-discovery", daemon=True)
        self._thread.start()
        log.info("discovery: answering NOD-CORE? on UDP %s", self.bound_port)
        return True

    def stop(self) -> None:
        s, self._sock = self._sock, None
        if s:
            s.close()
        self._thread = None

    def _run(self) -> None:
        while self._sock is not None:
            try:
                data, addr = self._sock.recvfrom(64)
            except socket.timeout:
                continue
            except OSError:
                return
            if data.strip() == QUERY:
                try:
                    self._sock.sendto(build_reply(self.port), addr)
                except OSError:
                    pass
