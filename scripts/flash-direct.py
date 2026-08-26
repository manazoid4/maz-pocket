#!/usr/bin/env python3
"""Write a new MAZ Pocket app image straight to its own OTA slot over USB.

This bypasses M5Launcher's hand-back entirely. It never touches the
Launcher partition, so Launcher's own recovery path stays intact -- it
only overwrites the same OTA slot MAZ Pocket is already running from,
the same way any ESP32 app-image upload would.

Use this when scripts/install.ps1's Launcher-mediated flash cannot
reach the Launcher menu (see docs/RELEASE_RULES.md, "Direct-flash
recovery path"). For a normal end-user install, use install.ps1 --
M5Launcher remains the product's installer.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path


def read_partition_table(port: str) -> bytes:
    tmp = Path(__file__).resolve().parent.parent / "host" / "_flash_direct_parts.bin"
    subprocess.run(
        [sys.executable, "-m", "esptool", "--port", port, "read_flash", "0x8000", "0x1000", str(tmp)],
        check=True, capture_output=True,
    )
    data = tmp.read_bytes()
    tmp.unlink()
    return data


def find_running_app_slot(table: bytes) -> tuple[int, int, str]:
    """Return (offset, size, label) of the app-type OTA slot MAZ Pocket owns.

    M5Launcher's own image lives in the TEST/FACTORY app slot (see
    src/core/launcher.cpp: handBackTarget prefers TEST, then FACTORY).
    MAZ Pocket's own slot is the other app-type partition -- an OTA_x
    subtype, labelled mazpoc/mazpoN by M5Launcher.
    """
    i = 0
    candidates = []
    while i < len(table):
        entry = table[i:i + 32]
        i += 32
        if len(entry) < 32 or entry[0:2] != b"\xAA\x50":
            continue
        ptype, psub = entry[2], entry[3]
        offset, size = struct.unpack("<II", entry[4:12])
        label = entry[12:28].split(b"\x00")[0].decode(errors="replace")
        if ptype == 0 and psub not in (0x00, 0x20):  # not FACTORY, not TEST
            candidates.append((offset, size, label))
    if not candidates:
        raise RuntimeError("no MAZ Pocket OTA app slot found in the live partition table")
    if len(candidates) > 1:
        raise RuntimeError(f"more than one candidate OTA slot found: {candidates}; resolve by hand")
    return candidates[0]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--binary", required=True, type=Path)
    args = parser.parse_args()

    image = args.binary.read_bytes()
    if len(image) < 4 or image[0] != 0xE9:
        raise SystemExit(f"{args.binary} is not a valid ESP32 app image (bad magic byte)")

    table = read_partition_table(args.port)
    offset, size, label = find_running_app_slot(table)
    print(f"[+] MAZ Pocket OTA slot: '{label}' at {hex(offset)}, size {size} bytes")
    if len(image) > size:
        raise SystemExit(f"{args.binary} is {len(image)} bytes; slot '{label}' only holds {size}")
    print(f"[+] New image is {len(image)} bytes, {size - len(image)} bytes of headroom")

    subprocess.run(
        [sys.executable, "-m", "esptool", "--port", args.port, "write_flash",
         "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep",
         hex(offset), str(args.binary)],
        check=True,
    )
    print("[+] Wrote and verified. Launcher partition was never touched.")


if __name__ == "__main__":
    main()
