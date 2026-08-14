#!/usr/bin/env python3
"""Small serial checks around M5Launcher's official firmware flasher."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time

import serial


BOOT_BANNER = "Press the button to enter the Launcher!"
READY_BANNER = "MAZ Pocket 0.2.0 READY"


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


def prepare(port: str) -> None:
    reset(port)
    with serial.Serial(port, 115200, timeout=0.2) as device:
        deadline = time.time() + 15
        while BOOT_BANNER not in read_line(device, deadline):
            if time.time() >= deadline:
                raise RuntimeError("M5Launcher boot banner not found")
        device.write(b"nav SelPress\n")
        device.flush()
        time.sleep(0.3)
        # A MAZ hand-back invalidates the old app image; remove its Launcher
        # table entry so upgrades reuse one slot instead of accumulating OTAs.
        device.write(b"partition delete mazpoc\n")
        device.flush()
        deadline = time.time() + 8
        while time.time() < deadline:
            line = read_line(device, deadline)
            if line.startswith("OK partition deleted") or line.startswith(
                "ERR partition not found"
            ):
                break
        device.write(b"partitions\n")
        device.flush()

        output: list[str] = []
        deadline = time.time() + 8
        while time.time() < deadline:
            line = read_line(device, deadline)
            output.append(line)
            if "Total free:" in line:
                break
        if any(line.startswith("mazdata ") for line in output):
            print("[+] M5Launcher MAZ storage is ready.")
            return

        device.write(b"partition create data littlefs mazdata 0x200000\n")
        device.flush()
        deadline = time.time() + 10
        while time.time() < deadline:
            line = read_line(device, deadline)
            if line.startswith("OK partition created"):
                print("[+] M5Launcher created dedicated MAZ storage.")
                return
            if line.startswith("ERR Duplicate partition label"):
                print("[+] M5Launcher MAZ storage is ready.")
                return
            if line.startswith("ERR"):
                raise RuntimeError(line)
        raise RuntimeError("M5Launcher did not confirm MAZ storage creation")


def verify(port: str) -> None:
    reset(port)
    with serial.Serial(port, 115200, timeout=0.2) as device:
        deadline = time.time() + 15
        while time.time() < deadline:
            line = read_line(device, deadline)
            if READY_BANNER in line:
                print("[+] MAZ Pocket boot verified on hardware.")
                return
        raise RuntimeError("MAZ Pocket READY banner not observed")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("prepare", "verify"))
    parser.add_argument("--port", required=True)
    args = parser.parse_args()
    (prepare if args.mode == "prepare" else verify)(args.port)


if __name__ == "__main__":
    main()
