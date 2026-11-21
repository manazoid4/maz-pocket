from __future__ import annotations

import queue
import threading
import tkinter as tk
import webbrowser
from tkinter import messagebox, ttk

from .desktop_runtime import CoreRuntime, api_get
from .pairing_usb import pair


class DesktopApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.runtime = CoreRuntime()
        self.results: queue.Queue = queue.Queue()
        root.title("MAZ Core")
        root.geometry("620x430")
        root.minsize(520, 360)
        root.configure(bg="#f6cf00")
        frame = ttk.Frame(root, padding=24)
        frame.pack(fill="both", expand=True, padx=12, pady=12)
        ttk.Label(frame, text="MAZ CORE", font=("Segoe UI", 24, "bold")).pack(anchor="w")
        ttk.Label(frame, text="Cardputer connection to local and cloud AI", font=("Segoe UI", 11)).pack(anchor="w", pady=(0, 20))
        self.core = tk.StringVar(value="Core: starting…")
        self.pocket = tk.StringVar(value="Pocket: checking…")
        self.route = tk.StringVar(value="AI route: checking…")
        for value in (self.core, self.pocket, self.route):
            ttk.Label(frame, textvariable=value, font=("Segoe UI", 12)).pack(anchor="w", pady=5)
        self.detail = tk.StringVar(value="")
        ttk.Label(frame, textvariable=self.detail, foreground="#8a1c00", wraplength=540).pack(anchor="w", pady=(10, 12))
        buttons = ttk.Frame(frame)
        buttons.pack(fill="x", pady=10)
        for text, action in (("PAIR USB", self.pair_usb), ("RETRY", self.refresh), ("RESTART CORE", self.restart), ("OPEN POCKET", self.open_pocket)):
            ttk.Button(buttons, text=text, command=action).pack(side="left", padx=(0, 8))
        ttk.Label(frame, text="USB pairing changes only the Core address and credential. Wi-Fi and saved captures stay intact.", wraplength=540).pack(anchor="w", side="bottom")
        self._poll()
        self._run(self._start_and_status)

    def _run(self, function) -> None:
        self.detail.set("Working…")
        threading.Thread(target=lambda: self._finish(function), daemon=True).start()

    def _finish(self, function) -> None:
        try:
            self.results.put((True, function()))
        except Exception as error:
            self.results.put((False, str(error)))

    def _poll(self) -> None:
        try:
            while True:
                ok, value = self.results.get_nowait()
                self.detail.set("" if ok else value)
                if ok and isinstance(value, dict):
                    self._render(value)
        except queue.Empty:
            pass
        self.root.after(150, self._poll)

    def _start_and_status(self) -> dict:
        self.runtime.start()
        return self._status()

    def _status(self) -> dict:
        values = self.runtime.values()
        return {"health": api_get(values, "/health"), "diagnostics": api_get(values, "/diagnostics"), "models": api_get(values, "/models")}

    def _render(self, status: dict) -> None:
        if status.get("paired"):
            self.detail.set(f"Paired {status['paired']['port']} to {status['paired']['address']}:{status['paired']['core_port']}")
            self.refresh()
            return
        health, diagnostics, models = status["health"], status["diagnostics"], status["models"]
        self.core.set(f"Core: online · v{health.get('version', '?')}")
        card = diagnostics.get("cardputer", {})
        self.pocket.set(f"Pocket USB: {card.get('status', 'unknown')}")
        self.route.set(f"AI route: {models.get('default_route', '?')} · last provider {diagnostics.get('routes', {}).get('last_provider') or 'not tested'}")

    def refresh(self) -> None:
        self._run(self._status)

    def restart(self) -> None:
        self._run(lambda: (self.runtime.restart(), self._status())[1])

    def pair_usb(self) -> None:
        self._run(lambda: {"paired": pair(self.runtime.values())})

    def open_pocket(self) -> None:
        webbrowser.open(self.runtime.values().get("MAZ_CARDPUTER_URL", "http://mazpocket.local"))


def main() -> None:
    root = tk.Tk()
    DesktopApp(root)
    root.mainloop()
