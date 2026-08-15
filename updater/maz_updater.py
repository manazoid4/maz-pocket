"""Maz Pocket transition updater 0.3.2.

One job only: safely replace the currently selected Maz Pocket OTA image while
leaving M5Launcher, the partition table, NVS/settings, SD data, and sibling
firmwares untouched.

The updater reads the live partition table + otadata first, saves a full recovery
copy of the active app slot, writes the new image in-place through the ESP32-S3
ROM loader, verifies it byte-for-byte, then performs a boot acceptance check.
A failed write/verification/boot check triggers an automatic restore when the
USB device is still reachable.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import socket
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from tkinter import BOTH, END, X, Button, Label, StringVar, Text, Tk, messagebox
from tkinter.ttk import Progressbar

import esptool
import serial
from serial.tools import list_ports

from flash_plan import (
    PARTITION_TABLE_OFFSET,
    PARTITION_TABLE_SIZE,
    find_otadata_partition,
    parse_partition_table,
    selected_ota_partition,
    sha256_bytes,
    validate_firmware,
)

VERSION = "0.3.2"
READY_VERSION = "0.3.0"
VID = 0x303A
PID = 0x1001
CONTROL_PORT = 8022
BAUD = 921600


class UpdateError(RuntimeError):
    pass


def resource_path(*parts: str) -> Path:
    root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return root.joinpath(*parts)


def firmware_path() -> Path:
    bundled = resource_path("firmware", "maz-pocket-app.bin")
    if bundled.is_file():
        return bundled
    local = Path(__file__).resolve().parents[1] / "dist" / "maz-pocket-app.bin"
    if local.is_file():
        return local
    raise UpdateError("Bundled Maz Pocket firmware is missing")


def recovery_dir() -> Path:
    root = Path(os.environ.get("APPDATA", Path.home())) / "MazPocket" / "recovery"
    root.mkdir(parents=True, exist_ok=True)
    return root


def matching_ports() -> list[str]:
    return [
        p.device for p in list_ports.comports()
        if p.vid == VID and p.pid == PID
    ]


def find_usb_port(preferred: str | None = None, wait: float = 0.0) -> str:
    deadline = time.time() + wait
    while True:
        ports = matching_ports()
        if preferred and preferred in ports:
            return preferred
        if len(ports) == 1:
            return ports[0]
        if len(ports) > 1:
            raise UpdateError(
                "More than one ESP32-S3 device is connected. Leave only the Cardputer connected and retry."
            )
        if time.time() >= deadline:
            raise UpdateError("Cardputer ADV not found. Connect its USB data cable and retry.")
        time.sleep(0.35)


def _esptool(args: list[str]) -> None:
    try:
        esptool.main(args)
    except SystemExit as exc:
        if exc.code not in (None, 0):
            raise UpdateError(f"ESP32 flasher exited with code {exc.code}") from exc
    except Exception as exc:
        raise UpdateError(str(exc)) from exc


def _base_args(port: str, after: str = "no_reset") -> list[str]:
    # Cardputer ADV native USB drops when the esptool RAM stub takes ownership.
    # --no-stub keeps the ROM transport stable on this exact hardware.
    return [
        "--chip", "esp32s3",
        "--port", port,
        "--baud", str(BAUD),
        "--before", "default_reset",
        "--after", after,
        "--no-stub",
    ]


def run_with_port_retry(port: str, command: list[str], *, after: str = "no_reset") -> str:
    last: Exception | None = None
    current = port
    for attempt in range(2):
        try:
            _esptool(_base_args(current, after) + command)
            return current
        except Exception as exc:
            last = exc
            if attempt == 0:
                time.sleep(0.7)
                current = find_usb_port(None, wait=4.0)
                continue
            break
    raise UpdateError(str(last) if last else "ESP32 flasher failed")


def read_flash(port: str, offset: int, size: int, path: Path) -> str:
    path.unlink(missing_ok=True)
    used = run_with_port_retry(
        port,
        ["read_flash", hex(offset), hex(size), str(path)],
    )
    if not path.is_file() or path.stat().st_size != size:
        raise UpdateError(
            f"Flash read verification failed at 0x{offset:x}: expected {size:,} bytes"
        )
    return used


def write_flash(port: str, offset: int, path: Path) -> str:
    if not path.is_file() or path.stat().st_size == 0:
        raise UpdateError("Refusing to flash an empty file")
    return run_with_port_retry(
        port,
        ["write_flash", hex(offset), str(path)],
    )


def reset_to_app(port: str) -> str:
    return run_with_port_retry(port, ["run"], after="hard_reset")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def serial_acceptance(preferred: str, seconds: float = 12.0) -> tuple[bool, str]:
    deadline = time.time() + seconds
    while time.time() < deadline:
        ports = matching_ports()
        ports.sort(key=lambda p: (p != preferred, p))
        for port in ports:
            try:
                with serial.Serial(port, 115200, timeout=0.35) as device:
                    time.sleep(0.25)
                    device.reset_input_buffer()
                    device.write(b"MAZPING\n")
                    device.flush()
                    until = time.time() + 1.2
                    while time.time() < until:
                        line = device.readline().decode(errors="replace").strip()
                        if f"MAZPING OK version={READY_VERSION}" in line:
                            return True, port
                        if f"MAZ Pocket {READY_VERSION} READY" in line:
                            return True, port
            except (serial.SerialException, OSError):
                pass
        time.sleep(0.45)
    return False, preferred


def local_ip() -> str | None:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("1.1.1.1", 80))
        return probe.getsockname()[0]
    except OSError:
        return None
    finally:
        probe.close()


def ping_v03(ip: str, timeout: float = 0.15) -> bool:
    try:
        with socket.create_connection((ip, CONTROL_PORT), timeout=timeout) as sock:
            sock.settimeout(timeout)
            try:
                sock.recv(256)
            except socket.timeout:
                pass
            sock.sendall(b"MAZPING\n")
            data = sock.recv(256).decode(errors="replace")
            return f"MAZPING OK version={READY_VERSION}" in data
    except OSError:
        return False


def lan_acceptance(seconds: float = 8.0) -> bool:
    mine = local_ip()
    if not mine or "." not in mine:
        return False
    prefix = mine.rsplit(".", 1)[0]
    deadline = time.time() + seconds
    while time.time() < deadline:
        candidates = [f"{prefix}.{i}" for i in range(1, 255)]
        with ThreadPoolExecutor(max_workers=48) as pool:
            futures = [pool.submit(ping_v03, ip) for ip in candidates]
            for future in as_completed(futures):
                if future.result():
                    return True
        time.sleep(0.7)
    return False


class App:
    def __init__(self) -> None:
        self.root = Tk()
        self.root.title(f"Maz Pocket Updater {VERSION}")
        self.root.geometry("590x420")
        self.root.minsize(530, 390)
        self.status = StringVar(value="Connect Cardputer by USB, then press UPDATE MAZ POCKET")
        self.busy = False

        Label(self.root, text="MAZ POCKET / SAFE UPDATE", font=("Consolas", 16, "bold")).pack(pady=(16, 4))
        Label(
            self.root,
            text="v0.02 → v0.03 • automatic backup • verify • rollback",
            font=("Consolas", 9),
        ).pack()

        self.button = Button(
            self.root,
            text="UPDATE MAZ POCKET",
            height=3,
            font=("Consolas", 12, "bold"),
            command=self.begin_update,
        )
        self.button.pack(fill=X, padx=18, pady=(18, 12))

        self.bar = Progressbar(self.root, maximum=100)
        self.bar.pack(fill=X, padx=18)
        Label(self.root, textvariable=self.status, anchor="w", font=("Consolas", 9)).pack(
            fill=X, padx=18, pady=(5, 6)
        )
        self.log = Text(
            self.root,
            height=13,
            font=("Consolas", 9),
            bg="#0b0d10",
            fg="#e6e9ec",
            insertbackground="white",
        )
        self.log.pack(fill=BOTH, expand=True, padx=18, pady=(0, 16))
        self.write("Updater 0.3.2 ready.")
        self.write("No FIND, token, IP, or M5Launcher screen is required.")

    def write(self, line: str) -> None:
        self.root.after(0, lambda: (self.log.insert(END, line + "\n"), self.log.see(END)))

    def progress(self, value: float, text: str) -> None:
        self.root.after(
            0,
            lambda: (
                self.bar.configure(value=max(0, min(100, value * 100))),
                self.status.set(text),
            ),
        )

    def set_busy(self, busy: bool) -> None:
        self.busy = busy
        self.root.after(0, lambda: self.button.configure(state="disabled" if busy else "normal"))

    def begin_update(self) -> None:
        if self.busy:
            return
        self.set_busy(True)
        threading.Thread(target=self._worker, daemon=True).start()

    def _worker(self) -> None:
        try:
            self.safe_update()
        except Exception as exc:
            self.write(f"ERROR: {exc}")
            self.progress(0, "Update stopped safely")
            self.root.after(0, lambda: messagebox.showerror("Maz Pocket", str(exc)))
        finally:
            self.set_busy(False)

    def _restore(self, port: str, target_offset: int, backup: Path, expected_sha: str) -> None:
        self.write("Recovery: restoring the exact previous flash state...")
        self.progress(0.62, "Automatic rollback")
        port = find_usb_port(port, wait=5.0)
        port = write_flash(port, target_offset, backup)
        with tempfile.TemporaryDirectory(prefix="maz-restore-") as temp:
            verify = Path(temp) / "restore-verify.bin"
            port = read_flash(port, target_offset, backup.stat().st_size, verify)
            if sha256_file(verify) != expected_sha:
                raise UpdateError(
                    "Automatic rollback could not be verified. Recovery backup is saved on this PC; do not erase the device."
                )
        reset_to_app(port)
        self.write("Recovery OK: previous flash state restored and verified.")

    def safe_update(self) -> None:
        fw = firmware_path()
        firmware = fw.read_bytes()
        firmware_sha = sha256_bytes(firmware)

        self.progress(0.02, "Finding Cardputer")
        port = find_usb_port(wait=2.0)
        self.write(f"Cardputer: {port}")
        self.write(f"New firmware: {len(firmware):,} bytes / SHA256 {firmware_sha[:12]}...")

        with tempfile.TemporaryDirectory(prefix="maz-pocket-update-") as temp_name:
            temp = Path(temp_name)

            self.progress(0.07, "Reading live partition map")
            pt_path = temp / "partitions.bin"
            port = read_flash(port, PARTITION_TABLE_OFFSET, PARTITION_TABLE_SIZE, pt_path)
            parts = parse_partition_table(pt_path.read_bytes())

            otapart = find_otadata_partition(parts)
            ota_path = temp / "otadata.bin"
            self.progress(0.12, "Finding the active Maz slot")
            port = read_flash(port, otapart.offset, otapart.size, ota_path)
            target = selected_ota_partition(parts, ota_path.read_bytes())
            validate_firmware(firmware, target)
            self.write(
                f"Selected app: {target.label or '<unnamed>'} @ 0x{target.offset:x} / {target.size:,} bytes"
            )
            self.write("Safety: partition table, NVS, Launcher and sibling apps will not be written.")

            self.progress(0.20, "Backing up previous flash state")
            backup_tmp = temp / "previous-slot.bin"
            port = read_flash(port, target.offset, target.size, backup_tmp)
            old_sha = sha256_file(backup_tmp)
            old_bytes = backup_tmp.read_bytes()
            label_is_maz = "maz" in target.label.lower()
            image_is_maz = b"maz pocket" in old_bytes.lower() or b"maz-pocket" in old_bytes.lower()
            if not label_is_maz and not image_is_maz:
                raise UpdateError(
                    f"Selected OTA slot '{target.label or '<unnamed>'}' does not look like Maz Pocket; nothing was erased"
                )
            if not old_bytes.startswith(b"\xE9"):
                self.write(
                    "Note: the previous updater already invalidated the Maz slot; "
                    "its exact pre-update state is still backed up for rollback."
                )

            stamp = time.strftime("%Y%m%d-%H%M%S")
            saved = recovery_dir() / f"maz-pocket-before-v03-{stamp}-0x{target.offset:x}.bin"
            saved.write_bytes(old_bytes)
            if sha256_file(saved) != old_sha:
                raise UpdateError("Recovery backup did not verify on disk; nothing was erased")
            meta = saved.with_suffix(".json")
            meta.write_text(
                json.dumps(
                    {
                        "created": stamp,
                        "port": port,
                        "partition_label": target.label,
                        "partition_offset": target.offset,
                        "partition_size": target.size,
                        "backup_sha256": old_sha,
                        "new_firmware_sha256": firmware_sha,
                        "new_firmware_size": len(firmware),
                    },
                    indent=2,
                ),
                encoding="utf-8",
            )
            self.write(f"Recovery backup verified: {saved.name}")

            wrote_new = False
            try:
                self.progress(0.43, "Writing Maz Pocket v0.03")
                port = write_flash(port, target.offset, fw)
                wrote_new = True

                self.progress(0.68, "Verifying flash byte-for-byte")
                verify_new = temp / "new-verify.bin"
                port = read_flash(port, target.offset, len(firmware), verify_new)
                actual_sha = sha256_file(verify_new)
                if actual_sha != firmware_sha:
                    raise UpdateError(
                        f"Flash SHA mismatch ({actual_sha[:12]} != {firmware_sha[:12]})"
                    )
                self.write("Flash verify OK / exact SHA256 match")

                self.progress(0.82, "Booting v0.03")
                port = reset_to_app(port)

                self.progress(0.87, "Checking v0.03 started correctly")
                serial_ok, accepted_port = serial_acceptance(port, 12.0)
                network_ok = False if serial_ok else lan_acceptance(6.0)
                if not serial_ok and not network_ok:
                    raise UpdateError("v0.03 did not pass its boot acceptance check")
                port = accepted_port

                self.progress(1.0, "Maz Pocket v0.03 ready")
                self.write(
                    "BOOT CHECK OK / " + ("USB" if serial_ok else "Wi-Fi") + " confirmed v0.03"
                )
                self.write("UPDATE COMPLETE / Wi-Fi and settings were preserved")
                self.root.after(
                    0,
                    lambda: messagebox.showinfo(
                        "Maz Pocket",
                        "Maz Pocket v0.03 is installed and verified.\n\nFuture updates can use mazpocket.local when the dashboard reports Browser OTA SAFE.",
                    ),
                )
            except Exception as failure:
                if wrote_new:
                    try:
                        self._restore(port, target.offset, saved, old_sha)
                    except Exception as restore_error:
                        raise UpdateError(
                            f"Update failed: {failure}\n\nRollback also needs attention: {restore_error}\n\nRecovery backup: {saved}"
                        ) from restore_error
                    raise UpdateError(
                        f"Update failed its verification/boot check, so the exact previous flash state was restored automatically.\n\nReason: {failure}"
                    ) from failure
                raise

    def mainloop(self) -> None:
        self.root.mainloop()


if __name__ == "__main__":
    App().mainloop()
