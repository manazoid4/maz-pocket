"""Compatibility entry point for the packaged v0.3 updater.

v0.02 predates MAZPING/port-8022 discovery. If its USB serial device is
present, FIND should say that the transition USB update is ready instead of
claiming Maz Pocket is missing. Everything else stays in maz_updater.py.
"""

from __future__ import annotations

import re

from maz_updater import App as BaseApp, discover_lan, find_usb_port, usb_command


class App(BaseApp):
    def __init__(self) -> None:
        super().__init__()
        self.root.title("Maz Pocket Updater 0.3.1")
        self.write("Updater 0.3.1 compatibility fix loaded.")

    def discover(self) -> None:
        port = find_usb_port()
        if port:
            self.write(f"USB: {port}")
            try:
                line = usb_command(port, "MAZPING", "MAZPING", 5)
                match = re.search(r"ip=([0-9.]+)", line)
                if match and match.group(1) != "0.0.0.0":
                    ip = match.group(1)
                    self.root.after(0, lambda: self.device_ip.set(ip))
                    self.progress(1, f"Found Maz Pocket at {ip}")
                    return
            except Exception:
                # Expected for v0.02: it has USB serial, but no MAZPING yet.
                self.write(f"Legacy Maz Pocket detected on {port} (v0.02 may not support FIND).")
                self.write("Ready: click USB UPDATE. No host token is required for this step.")
                self.progress(1, f"Legacy device ready on {port} / click USB UPDATE")
                return

        found = discover_lan(self.progress)
        if not found:
            raise RuntimeError("Maz Pocket was not found over USB or this Wi-Fi network")
        ip, banner = found
        self.root.after(0, lambda: self.device_ip.set(ip))
        self.write(banner)
        self.progress(1, f"Found Maz Pocket at {ip}")


if __name__ == "__main__":
    App().mainloop()
