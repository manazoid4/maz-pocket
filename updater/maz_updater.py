"""Maz Pocket Updater — one small Windows surface for v0.3.

USB uses M5Launcher's existing serial installer so it preserves the launcher and
MAZ storage partition. Wi-Fi uses ArduinoOTA directly. Pairing and optional
Tailscale Funnel provisioning live here as well, because setup should not
require remembering separate scripts.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import runpy
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from tkinter import END, BOTH, LEFT, RIGHT, X, Button, Entry, Frame, Label, StringVar, Text, Tk, messagebox, simpledialog
from tkinter.ttk import Progressbar

import serial
from serial.tools import list_ports

import esptool

from ota import upload as ota_upload


VERSION = "0.3.0"
VID = 0x303A
PID = 0x1001
CONTROL_PORT = 8022
LAUNCHER_TOOL_URL = "https://raw.githubusercontent.com/bmorcelli/M5Stick-Launcher/2.8.0/tools/serial_flasher.py"
LAUNCHER_TOOL_SHA256 = "9CFBA9AF762AC7D99488F23706320B30D0896C4993599990EC80AD06AB8F7536"
BOOT_BANNER = "Press the button to enter the Launcher!"
READY_BANNER = "MAZ Pocket 0.3.0 READY"


def resource_path(*parts: str) -> Path:
    root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return root.joinpath(*parts)


def config_path() -> Path:
    root = Path(os.environ.get("APPDATA", Path.home())) / "MazPocket"
    root.mkdir(parents=True, exist_ok=True)
    return root / "updater.json"


def load_config() -> dict:
    path = config_path()
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def save_config(data: dict) -> None:
    config_path().write_text(json.dumps(data, indent=2), encoding="utf-8")


def find_usb_port() -> str | None:
    for port in list_ports.comports():
        if port.vid == VID and port.pid == PID:
            return port.device
    return None


def reset_device(port: str) -> None:
    try:
        esptool.main(["--port", port, "--after", "hard_reset", "run"])
    except SystemExit as exc:
        if exc.code not in (None, 0):
            raise RuntimeError(f"ESP reset failed ({exc.code})") from exc


def read_until(device: serial.Serial, text: str, seconds: float) -> bool:
    deadline = time.time() + seconds
    while time.time() < deadline:
        line = device.readline().decode(errors="replace").strip()
        if text in line:
            return True
    return False


def launcher_handoff(port: str) -> None:
    try:
        with serial.Serial(port, 115200, timeout=0.2) as device:
            read_until(device, "MAZ Pocket", 4)
            device.write(b"MAZLAUNCHER\n")
            device.flush()
            if read_until(device, "MAZLAUNCHER OK", 4):
                time.sleep(2)
    except (serial.SerialException, OSError):
        pass


def launcher_prepare(port: str) -> None:
    reset_device(port)
    with serial.Serial(port, 115200, timeout=0.2) as device:
        if not read_until(device, BOOT_BANNER, 15):
            raise RuntimeError("M5Launcher boot banner not found. Open Launcher once, then retry.")
        device.write(b"nav SelPress\n")
        device.flush()
        time.sleep(0.3)
        device.write(b"partition delete mazpoc\n")
        device.flush()
        time.sleep(0.5)
        device.write(b"partitions\n")
        device.flush()

        output: list[str] = []
        deadline = time.time() + 7
        while time.time() < deadline:
            line = device.readline().decode(errors="replace").strip()
            if line:
                output.append(line)
            if "Total free:" in line:
                break
        if any(line.startswith("mazdata ") for line in output):
            return

        device.write(b"partition create data littlefs mazdata 0x200000\n")
        device.flush()
        deadline = time.time() + 10
        while time.time() < deadline:
            line = device.readline().decode(errors="replace").strip()
            if line.startswith("OK partition created") or line.startswith("ERR Duplicate partition label"):
                return
            if line.startswith("ERR"):
                raise RuntimeError(line)
        raise RuntimeError("M5Launcher did not create MAZ storage")


def cached_launcher_flasher() -> Path:
    target = Path(tempfile.gettempdir()) / "maz-pocket-launcher-2.8.0" / "serial_flasher.py"
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest().upper() != LAUNCHER_TOOL_SHA256:
        urllib.request.urlretrieve(LAUNCHER_TOOL_URL, target)
    actual = hashlib.sha256(target.read_bytes()).hexdigest().upper()
    if actual != LAUNCHER_TOOL_SHA256:
        target.unlink(missing_ok=True)
        raise RuntimeError("M5Launcher helper failed checksum verification")
    return target


def launcher_flash(port: str, firmware: Path) -> None:
    helper = cached_launcher_flasher()
    old_argv = sys.argv[:]
    try:
        sys.argv = [str(helper), "-f", str(firmware), "-p", port, "-n", "MAZ-Pocket"]
        try:
            runpy.run_path(str(helper), run_name="__main__")
        except SystemExit as exc:
            if exc.code not in (None, 0):
                raise RuntimeError(f"M5Launcher installer exited with {exc.code}") from exc
    finally:
        sys.argv = old_argv


def verify_usb(port: str) -> None:
    reset_device(port)
    with serial.Serial(port, 115200, timeout=0.2) as device:
        if not read_until(device, READY_BANNER, 18):
            raise RuntimeError("v0.3 READY banner was not observed after flash")


def usb_command(port: str, command: str, prefix: str, timeout: float = 15) -> str:
    with serial.Serial(port, 115200, timeout=0.25) as device:
        time.sleep(0.35)
        device.reset_input_buffer()
        device.write((command + "\n").encode())
        device.flush()
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = device.readline().decode(errors="replace").strip()
            if line.startswith(prefix):
                return line
    raise RuntimeError(f"No {prefix} response from Cardputer")


def local_ip() -> str:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("1.1.1.1", 80))
        return probe.getsockname()[0]
    finally:
        probe.close()


def ping_control(ip: str, timeout: float = 0.12) -> str | None:
    try:
        with socket.create_connection((ip, CONTROL_PORT), timeout=timeout) as sock:
            sock.settimeout(timeout)
            try:
                sock.recv(256)
            except socket.timeout:
                pass
            sock.sendall(b"MAZPING\n")
            data = sock.recv(256).decode(errors="replace").strip()
            return data if data.startswith("MAZPING OK") else None
    except OSError:
        return None


def discover_lan(progress=None) -> tuple[str, str] | None:
    mine = local_ip()
    prefix = mine.rsplit(".", 1)[0]
    candidates = [f"{prefix}.{i}" for i in range(1, 255) if f"{prefix}.{i}" != mine]
    with ThreadPoolExecutor(max_workers=64) as pool:
        futures = {pool.submit(ping_control, ip): ip for ip in candidates}
        done = 0
        for future in as_completed(futures):
            done += 1
            if progress and done % 30 == 0:
                progress(done / len(candidates), "Looking for Maz Pocket on Wi-Fi")
            result = future.result()
            if result:
                return futures[future], result
    return None


def control_command(ip: str, token: str, command: str) -> str:
    with socket.create_connection((ip, CONTROL_PORT), timeout=3) as sock:
        stream = sock.makefile("rwb", buffering=0)
        stream.readline()
        stream.write(f"MAZAUTH\t{token}\n".encode())
        auth = stream.readline().decode(errors="replace").strip()
        if auth != "MAZAUTH OK":
            raise RuntimeError("Cardputer control token was rejected")
        stream.write((command + "\n").encode())
        return stream.readline().decode(errors="replace").strip()


def tailscale_path() -> str | None:
    found = shutil.which("tailscale") or shutil.which("tailscale.exe")
    if found:
        return found
    candidate = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Tailscale" / "tailscale.exe"
    return str(candidate) if candidate.exists() else None


def enable_funnel() -> str:
    tailscale = tailscale_path()
    if not tailscale:
        raise RuntimeError("Tailscale is not installed. Install/sign in to Tailscale, then retry.")
    result = subprocess.run(
        [tailscale, "funnel", "--bg", "8787"], capture_output=True, text=True, timeout=20
    )
    text = (result.stdout or "") + "\n" + (result.stderr or "")
    if result.returncode != 0:
        raise RuntimeError(text.strip() or "Tailscale Funnel failed")
    match = re.search(r"https://[a-zA-Z0-9.-]+\.ts\.net(?::\d+)?", text)
    if not match:
        status = subprocess.run([tailscale, "funnel", "status"], capture_output=True, text=True, timeout=10)
        text += "\n" + status.stdout + "\n" + status.stderr
        match = re.search(r"https://[a-zA-Z0-9.-]+\.ts\.net(?::\d+)?", text)
    if not match:
        raise RuntimeError("Funnel started but its HTTPS address could not be detected")
    return match.group(0).rstrip("/")


class App:
    def __init__(self) -> None:
        self.root = Tk()
        self.root.title(f"Maz Pocket Updater {VERSION}")
        self.root.geometry("600x430")
        self.root.minsize(540, 390)
        self.cfg = load_config()
        self.device_ip = StringVar(value=self.cfg.get("device_ip", ""))
        self.token = StringVar(value=self.cfg.get("token", ""))
        self.status = StringVar(value="Ready")

        Label(self.root, text="MAZ POCKET / UPDATE CONSOLE", font=("Consolas", 16, "bold")).pack(pady=(14, 4))
        Label(self.root, text="USB install • Wi-Fi OTA • pairing • remote Call PC", font=("Consolas", 9)).pack()

        device = Frame(self.root)
        device.pack(fill=X, padx=16, pady=12)
        Label(device, text="Device IP", width=10, anchor="w").pack(side=LEFT)
        Entry(device, textvariable=self.device_ip).pack(side=LEFT, fill=X, expand=True, padx=(0, 8))
        Button(device, text="FIND", command=lambda: self.run(self.discover)).pack(side=RIGHT)

        token = Frame(self.root)
        token.pack(fill=X, padx=16)
        Label(token, text="Host token", width=10, anchor="w").pack(side=LEFT)
        Entry(token, textvariable=self.token, show="•").pack(side=LEFT, fill=X, expand=True)

        buttons = Frame(self.root)
        buttons.pack(fill=X, padx=16, pady=14)
        Button(buttons, text="USB UPDATE", height=2, command=lambda: self.run(self.usb_update)).pack(side=LEFT, fill=X, expand=True, padx=(0, 5))
        Button(buttons, text="WI-FI UPDATE", height=2, command=lambda: self.run(self.wifi_update)).pack(side=LEFT, fill=X, expand=True, padx=5)
        Button(buttons, text="PAIR", height=2, command=lambda: self.run(self.pair)).pack(side=LEFT, fill=X, expand=True, padx=5)
        Button(buttons, text="REMOTE CALL", height=2, command=lambda: self.run(self.remote_call)).pack(side=LEFT, fill=X, expand=True, padx=(5, 0))

        self.bar = Progressbar(self.root, maximum=100)
        self.bar.pack(fill=X, padx=16)
        Label(self.root, textvariable=self.status, anchor="w", font=("Consolas", 9)).pack(fill=X, padx=16, pady=(4, 5))
        self.log = Text(self.root, height=10, font=("Consolas", 9), bg="#0b0d10", fg="#e6e9ec", insertbackground="white")
        self.log.pack(fill=BOTH, expand=True, padx=16, pady=(0, 14))
        self.write("v0.3 updater ready. USB is safest for first install; Wi-Fi is fastest after pairing.")

    @property
    def firmware(self) -> Path:
        path = resource_path("firmware", "maz-pocket-app.bin")
        if not path.exists():
            # Developer mode: run directly from updater/ while dist exists.
            candidate = Path(__file__).resolve().parents[1] / "dist" / "maz-pocket-app.bin"
            if candidate.exists():
                return candidate
            raise RuntimeError("Bundled firmware image is missing")
        return path

    def write(self, line: str) -> None:
        self.root.after(0, lambda: (self.log.insert(END, line + "\n"), self.log.see(END)))

    def progress(self, value: float, text: str) -> None:
        self.root.after(0, lambda: (self.bar.configure(value=max(0, min(100, value * 100))), self.status.set(text)))

    def remember(self) -> None:
        self.cfg["device_ip"] = self.device_ip.get().strip()
        self.cfg["token"] = self.token.get().strip()
        save_config(self.cfg)

    def run(self, fn) -> None:
        def worker():
            try:
                self.progress(0, "Working...")
                fn()
            except Exception as exc:
                self.write(f"ERROR: {exc}")
                self.progress(0, "Failed")
                self.root.after(0, lambda: messagebox.showerror("Maz Pocket", str(exc)))
            finally:
                self.remember()
        threading.Thread(target=worker, daemon=True).start()

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
                pass
        found = discover_lan(self.progress)
        if not found:
            raise RuntimeError("Maz Pocket was not found over USB or this Wi-Fi network")
        ip, banner = found
        self.root.after(0, lambda: self.device_ip.set(ip))
        self.write(banner)
        self.progress(1, f"Found Maz Pocket at {ip}")

    def usb_update(self) -> None:
        port = find_usb_port()
        if not port:
            raise RuntimeError("Cardputer ADV not found. Connect a USB data cable and tap RESET.")
        self.write(f"USB update on {port}")
        self.progress(0.05, "Handing back to M5Launcher")
        launcher_handoff(port)
        self.progress(0.15, "Preparing Launcher partition")
        launcher_prepare(port)
        self.progress(0.3, "Installing Maz Pocket v0.3")
        launcher_flash(port, self.firmware)
        self.progress(0.9, "Verifying boot")
        verify_usb(port)
        self.progress(1, "USB update complete")
        self.write("USB UPDATE OK / v0.3 boot verified")

    def wifi_update(self) -> None:
        ip = self.device_ip.get().strip()
        token = self.token.get().strip()
        if not ip:
            found = discover_lan(self.progress)
            if not found:
                raise RuntimeError("Device IP is unknown; click FIND or connect USB")
            ip = found[0]
            self.root.after(0, lambda: self.device_ip.set(ip))
        if not token:
            raise RuntimeError("Enter the MAZ Host pairing token first")
        self.write(f"Wi-Fi OTA -> {ip}")
        ota_upload(ip, token, self.firmware, self.progress)
        self.write("WI-FI UPDATE OK / device rebooting")

    def pair(self) -> None:
        port = find_usb_port()
        if not port:
            raise RuntimeError("Pairing needs USB once so Wi-Fi credentials never cross an unauthenticated network")
        ssid = simpledialog.askstring("Pair Maz Pocket", "2.4 GHz Wi-Fi name:", parent=self.root)
        if not ssid:
            raise RuntimeError("Pairing cancelled")
        password = simpledialog.askstring("Pair Maz Pocket", f"Password for {ssid}:", show="•", parent=self.root)
        if password is None:
            raise RuntimeError("Pairing cancelled")
        token = self.token.get().strip() or secrets.token_urlsafe(24)
        host_ip = local_ip()
        command = f"MAZPAIR\t{ssid}\t{password}\t{host_ip}\t8787\t{token}"
        reply = usb_command(port, command, "MAZPAIR", 20)
        if not reply.startswith("MAZPAIR OK"):
            raise RuntimeError(reply)
        self.root.after(0, lambda: self.token.set(token))
        self.write(reply)
        time.sleep(1)
        try:
            ping = usb_command(port, "MAZPING", "MAZPING", 5)
            match = re.search(r"ip=([0-9.]+)", ping)
            if match:
                self.root.after(0, lambda: self.device_ip.set(match.group(1)))
        except Exception:
            pass
        self.progress(1, f"Paired to MAZ Host at {host_ip}:8787")

    def remote_call(self) -> None:
        token = self.token.get().strip()
        if not token:
            raise RuntimeError("Pair the Cardputer first so it has a host token")
        self.progress(0.15, "Starting Tailscale Funnel")
        url = enable_funnel()
        self.write(f"Remote Call PC: {url}")
        command = f"MAZREMOTE\t{url}"
        port = find_usb_port()
        if port:
            reply = usb_command(port, command, "MAZREMOTE", 8)
        else:
            ip = self.device_ip.get().strip()
            if not ip:
                raise RuntimeError("Connect USB or enter the Cardputer IP to provision remote access")
            reply = control_command(ip, token, command)
        if not reply.startswith("MAZREMOTE OK"):
            raise RuntimeError(reply)
        self.cfg["remote_url"] = url
        self.progress(1, "Remote Call PC enabled")
        self.write("REMOTE CALL OK / LAN remains preferred, HTTPS is fallback")

    def mainloop(self) -> None:
        self.root.mainloop()


if __name__ == "__main__":
    App().mainloop()
