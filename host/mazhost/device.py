from __future__ import annotations

from collections import deque
from dataclasses import asdict, dataclass
from threading import Event, Lock, Thread
from time import time

import serial
from serial.tools import list_ports

from .config import Settings


@dataclass
class DeviceState:
    monitoring: bool = False
    connection_state: str = "stopped"
    port: str = ""
    reconnect_attempts: int = 0
    successful_reconnects: int = 0
    last_seen_at: float | None = None
    last_error: str = ""


def select_port(configured: str, vid: int, pid: int, ports=None) -> str:
    if configured:
        return configured
    for port in ports if ports is not None else list_ports.comports():
        if port.vid == vid and port.pid == pid:
            return port.device
    return ""


class DeviceMonitor:
    """Bounded serial monitor that follows the Cardputer across USB resets.

    It is opt-in because owning COM5 while Launcher is installing firmware is
    harmful. Starting and stopping are explicit authenticated host actions.
    """

    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.state = DeviceState()
        self._lines: deque[dict] = deque(maxlen=500)
        self._stop = Event()
        self._lock = Lock()
        self._thread: Thread | None = None

    def start(self) -> dict:
        with self._lock:
            if self._thread and self._thread.is_alive():
                return self.status()
            self._stop.clear()
            self.state.monitoring = True
            self.state.connection_state = "port_missing"
            self._thread = Thread(target=self._run, name="maz-device-monitor", daemon=True)
            self._thread.start()
        return self.status()

    def stop(self) -> dict:
        self._stop.set()
        thread = self._thread
        if thread and thread.is_alive():
            thread.join(timeout=2)
        self.state.monitoring = False
        self.state.connection_state = "stopped"
        return self.status()

    def status(self) -> dict:
        return asdict(self.state)

    def logs(self, limit: int = 100) -> list[dict]:
        return list(self._lines)[-limit:]

    def _run(self) -> None:
        while not self._stop.is_set():
            port = select_port(
                self.settings.device_port,
                self.settings.device_vid,
                self.settings.device_pid,
            )
            if not port:
                self.state.connection_state = "port_missing"
                self.state.reconnect_attempts += 1
                self._stop.wait(0.25)
                continue
            self.state.port = port
            try:
                with serial.Serial(port, self.settings.device_baud, timeout=0.25) as connection:
                    self.state.connection_state = "connected"
                    self.state.successful_reconnects += 1
                    self.state.last_error = ""
                    while not self._stop.is_set():
                        raw = connection.readline()
                        if not raw:
                            continue
                        self.state.last_seen_at = time()
                        self._lines.append({
                            "at": self.state.last_seen_at,
                            "text": raw.decode("utf-8", errors="replace").rstrip(),
                        })
            except (OSError, serial.SerialException) as error:
                self.state.connection_state = "reconnecting"
                self.state.reconnect_attempts += 1
                self.state.last_error = str(error)
                self._stop.wait(0.25)
