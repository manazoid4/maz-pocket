#!/usr/bin/env python3
"""Small serial checks around M5Launcher's official firmware flasher."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

import serial


BOOT_BANNER = "Press the button to enter the Launcher!"


def ready_banner() -> str:
    """The banner the firmware being installed will print.

    This was pinned to "MAZ Pocket 0.3.0 READY" for four releases, so `verify`
    could only ever pass on v0.3 hardware. Read the version being shipped
    instead: a stale acceptance check is worse than none, because it reports a
    successful flash as a failure and invites a re-flash of the same image.
    """
    version = (Path(__file__).resolve().parent.parent / "VERSION").read_text().strip()
    return f"MAZ Pocket {version} READY"


def reset(port: str) -> None:
    subprocess.run(
        [sys.executable, "-m", "esptool", "--port", port, "--after", "hard_reset", "run"],
        check=True,
        capture_output=True,
    )


def read_line(device: serial.Serial, deadline: float) -> str:
    while time.time() < deadline:
        line = device.readline().decode(errors="replace").strip()
        if line:
            print(f"    < {line}")
            return line
    return ""


def handoff(port: str) -> None:
    with serial.Serial(port, 115200, timeout=0.2) as device:
        deadline = time.time() + 15
        while time.time() < deadline:
            line = read_line(device, deadline)
            if "MAZ Pocket" in line and "READY" in line:
                break
        device.write(b"MAZLAUNCHER\n")
        device.flush()
        deadline = time.time() + 12
        next_navigation = time.time() + 0.15
        saw_launcher = False
        while time.time() < deadline:
            line = read_line(device, min(deadline, time.time() + 0.35))
            if line:
                if line.startswith("MAZLAUNCHER ERR"):
                    raise RuntimeError(line)
                if BOOT_BANNER in line:
                    saw_launcher = True
                    device.write(b"nav SelPress\n")
                    device.flush()
                    next_navigation = time.time() + 0.35
                    continue
                if saw_launcher and line.startswith("OK nav"):
                    print("[+] M5Launcher banner and serial navigation confirmed.")
                    return
            if time.time() >= next_navigation:
                try:
                    device.write(b"nav SelPress\n")
                    device.flush()
                except serial.SerialException:
                    pass
                next_navigation = time.time() + 0.35
        raise RuntimeError("M5Launcher banner/navigation was not observed after hand-back")


FREE_TOTAL = re.compile(r"free total:\s*(\d+)KB", re.IGNORECASE)


def command(device: serial.Serial, cmd: str, seconds: float,
            until: tuple[str, ...] = ()) -> list[str]:
    """Send one M5Launcher serial command and collect its reply lines."""
    print(f"    > {cmd}")
    device.write((cmd + "\n").encode())
    device.flush()
    lines: list[str] = []
    deadline = time.time() + seconds
    while time.time() < deadline:
        line = read_line(device, deadline)
        if not line:
            continue
        lines.append(line)
        if until and line.startswith(until):
            break
    return lines


def partitions(device: serial.Serial) -> tuple[list[str], int]:
    """Return the partition lines and free bytes M5Launcher reports.

    The old code waited for a "Total free:" line. The device prints
    "Flash size: 8MB, free total: 192KB", so that wait always ran to its
    timeout and the free space was never read at all -- which is how a flash
    could be attempted with 192KB free and fail as an unexplained timeout.
    """
    lines = command(device, "partitions", 8, until=("Flash size:",))
    free = 0
    for line in lines:
        found = FREE_TOTAL.search(line)
        if found:
            free = int(found.group(1)) * 1024
    return lines, free


def stale_app_labels(lines: list[str]) -> list[str]:
    """MAZ Pocket app slots left by a previous install.

    M5Launcher derives the label from the app name, and truncates/uniquifies
    it: installing "MAZ-Pocket" has produced both `mazpoc` and `mazpo2`. The
    old code deleted the literal `mazpoc` only, so a device carrying `mazpo2`
    kept a 4032KB slot and the new image had nowhere to go.
    """
    labels = []
    for line in lines:
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith("mazpo") and parts[1] == "type=app":
            labels.append(parts[0])
    return labels


def prepare(port: str, require_free: int = 0, already_in_launcher: bool = False) -> None:
    if not already_in_launcher:
        reset(port)
    with serial.Serial(port, 115200, timeout=0.2) as device:
        if not already_in_launcher:
            deadline = time.time() + 15
            while BOOT_BANNER not in read_line(device, deadline):
                if time.time() >= deadline:
                    raise RuntimeError("M5Launcher boot banner not found")
            command(device, "nav SelPress", 2, until=("OK nav",))

        lines, free = partitions(device)
        # Never infer ownership from a label prefix or remove a working app to
        # discover whether its replacement fits. A constrained layout needs an
        # explicitly verified backup/recovery path, not automatic repartitioning.
        if not any(line.startswith("mazdata ") and "type=data" in line for line in lines):
            raise RuntimeError("Existing MAZ storage was not identified; no partitions were changed.")
        required = ((require_free + 0xFFFF) // 0x10000) * 0x10000
        if require_free <= 0 or free < required:
            raise RuntimeError(
                f"Insufficient free flash: {free // 1024}KB available, "
                f"{required // 1024}KB aligned image required. "
                "No partitions were changed. Preserve the current app and storage; "
                "use a verified backup/recovery path before replacing an existing slot.")
        print(f"[+] Existing apps/storage preserved; {free // 1024}KB free.")


def inspect(port: str) -> None:
    """Ask an already-running Launcher for its map without reset/navigation."""
    device = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    device.dtr = False
    device.rts = False
    device.port = port
    with device:
        lines, free = partitions(device)
        if not any(line.startswith("Flash size:") for line in lines):
            raise RuntimeError("No Launcher partition report; leave the device in M5Launcher and retry.")
        print(f"[+] Read-only Launcher report: {free // 1024}KB free; no partitions changed.")


def verify(port: str) -> None:
    banner = ready_banner()
    reset(port)
    with serial.Serial(port, 115200, timeout=0.2) as device:
        deadline = time.time() + 20
        while time.time() < deadline:
            line = read_line(device, deadline)
            if banner in line:
                # The banner carries the boot evidence the device gate cares
                # about first: keyboard=ok means the TCA8418 expander came up.
                print(f"[+] Hardware boot verified: {line}")
                if "keyboard=ok" not in line:
                    raise RuntimeError(f"Booted but keyboard did not initialise: {line}")
                return
        raise RuntimeError(f"'{banner}' not observed within 20s")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("handoff", "inspect", "prepare", "verify"))
    parser.add_argument("--port", required=True)
    parser.add_argument("--require-free", type=int, default=0,
                        help="bytes the image needs; prepare fails early if the "
                        "device cannot fit it")
    parser.add_argument("--already-in-launcher", action="store_true",
                        help="keep the verified launcher session from handoff")
    args = parser.parse_args()
    if args.mode == "prepare":
        prepare(args.port, args.require_free, args.already_in_launcher)
    else:
        {"handoff": handoff, "inspect": inspect, "verify": verify}[args.mode](args.port)


if __name__ == "__main__":
    main()
