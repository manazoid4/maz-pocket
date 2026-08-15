"""Pure helpers for the Maz Pocket USB transition updater.

The updater deliberately plans against the *live* partition table instead of
assuming the PlatformIO development layout. M5Launcher can resize/move OTA
partitions, so hard-coded offsets are not safe on a real device.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import struct
import zlib

FLASH_SIZE = 8 * 1024 * 1024
PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0x1000
PARTITION_ENTRY_SIZE = 32
PARTITION_MAGIC = 0x50AA
PARTITION_MAGIC_MD5 = 0xEBEB
TYPE_APP = 0x00
TYPE_DATA = 0x01
SUBTYPE_DATA_OTA = 0x00
SUBTYPE_APP_OTA_MIN = 0x10
SUBTYPE_APP_OTA_MAX = 0x20
UINT32_MAX = 0xFFFFFFFF
ESP_IMAGE_MAGIC = 0xE9
ESP_IMAGE_MAX_SEGMENTS = 16


class PlanError(RuntimeError):
    """The live flash layout is not safe for an automatic update."""


@dataclass(frozen=True)
class Partition:
    type: int
    subtype: int
    offset: int
    size: int
    label: str
    flags: int = 0

    @property
    def end(self) -> int:
        return self.offset + self.size

    @property
    def is_ota_app(self) -> bool:
        return (
            self.type == TYPE_APP
            and SUBTYPE_APP_OTA_MIN <= self.subtype < SUBTYPE_APP_OTA_MAX
        )


def parse_partition_table(data: bytes) -> list[Partition]:
    if len(data) < PARTITION_TABLE_SIZE:
        raise PlanError("partition table read was incomplete")

    parts: list[Partition] = []
    for pos in range(0, PARTITION_TABLE_SIZE, PARTITION_ENTRY_SIZE):
        entry = data[pos : pos + PARTITION_ENTRY_SIZE]
        magic = struct.unpack_from("<H", entry, 0)[0]
        if magic in (0xFFFF, PARTITION_MAGIC_MD5):
            break
        if magic != PARTITION_MAGIC:
            raise PlanError(f"invalid partition entry at 0x{pos:x}")

        ptype, subtype, offset, size, raw_label, flags = struct.unpack(
            "<BBII16sI", entry[2:]
        )
        label = raw_label.split(b"\0", 1)[0].decode("utf-8", errors="replace")
        if size <= 0 or offset < 0x9000 or offset + size > FLASH_SIZE:
            raise PlanError(f"unsafe partition bounds for {label or '<unnamed>'}")
        parts.append(Partition(ptype, subtype, offset, size, label, flags))

    if not parts:
        raise PlanError("no partitions found")

    ordered = sorted(parts, key=lambda p: p.offset)
    for prev, cur in zip(ordered, ordered[1:]):
        if cur.offset < prev.end:
            raise PlanError(
                f"overlapping partitions: {prev.label or hex(prev.offset)} and "
                f"{cur.label or hex(cur.offset)}"
            )
    return parts


def ota_select_crc(seq: int) -> int:
    # Espressif bootloader_common_ota_select_crc() is equivalent to zlib CRC32
    # over the little-endian ota_seq, seeded with UINT32_MAX.
    return zlib.crc32(struct.pack("<I", seq & UINT32_MAX), UINT32_MAX) & UINT32_MAX


@dataclass(frozen=True)
class OtaEntry:
    seq: int
    state: int
    crc: int

    @property
    def valid(self) -> bool:
        if self.seq in (0, UINT32_MAX):
            return False
        if self.crc != ota_select_crc(self.seq):
            return False
        return self.state not in (3, 4)


def parse_ota_entry(block: bytes) -> OtaEntry:
    if len(block) < 32:
        raise PlanError("otadata entry was incomplete")
    seq = struct.unpack_from("<I", block, 0)[0]
    state = struct.unpack_from("<I", block, 24)[0]
    crc = struct.unpack_from("<I", block, 28)[0]
    return OtaEntry(seq, state, crc)


def find_otadata_partition(parts: list[Partition]) -> Partition:
    found = [p for p in parts if p.type == TYPE_DATA and p.subtype == SUBTYPE_DATA_OTA]
    if len(found) != 1:
        raise PlanError("expected exactly one otadata partition")
    if found[0].size < 0x2000:
        raise PlanError("otadata partition is too small")
    return found[0]


def selected_ota_partition(parts: list[Partition], otadata: bytes) -> Partition:
    if len(otadata) < 0x2000:
        raise PlanError("otadata read was incomplete")

    entries = [parse_ota_entry(otadata[0:32]), parse_ota_entry(otadata[0x1000:0x1020])]
    valid = [entry for entry in entries if entry.valid]
    if not valid:
        raise PlanError("no valid selected OTA slot was found")

    active = max(valid, key=lambda entry: entry.seq)
    ota_apps = [p for p in parts if p.is_ota_app]
    if not ota_apps:
        raise PlanError("no OTA app partitions exist")
    ota_count = len(ota_apps)
    slot = (active.seq - 1) % ota_count
    subtype = SUBTYPE_APP_OTA_MIN + slot
    matches = [p for p in ota_apps if p.subtype == subtype]
    if len(matches) != 1:
        raise PlanError(f"selected OTA subtype 0x{subtype:02x} is missing or ambiguous")
    return matches[0]


def measure_esp_image_size(data: bytes) -> int:
    if len(data) < 24 or data[0] != ESP_IMAGE_MAGIC:
        return 0
    segment_count = data[1]
    if segment_count == 0 or segment_count > ESP_IMAGE_MAX_SEGMENTS:
        return 0
    hash_appended = data[23] != 0
    cursor = 24
    for _ in range(segment_count):
        if cursor + 8 > len(data):
            return 0
        segment_size = struct.unpack_from("<I", data, cursor + 4)[0]
        cursor += 8
        if segment_size > len(data) or cursor + segment_size > len(data):
            return 0
        cursor += segment_size
    cursor = ((cursor + 15) // 16) * 16 + 1
    if hash_appended:
        cursor += 32
    cursor = ((cursor + 15) // 16) * 16
    return cursor if cursor <= len(data) else 0


def validate_firmware(data: bytes, target: Partition) -> None:
    if not target.is_ota_app:
        raise PlanError("selected target is not an OTA app partition")
    if not data or data[0] != ESP_IMAGE_MAGIC:
        raise PlanError("firmware is not an ESP32 application image")
    measured = measure_esp_image_size(data)
    if measured == 0:
        raise PlanError("firmware image structure is invalid or truncated")
    if len(data) > target.size:
        raise PlanError(
            f"firmware is {len(data):,} bytes but the live Maz slot is only "
            f"{target.size:,} bytes; nothing was erased"
        )


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()
