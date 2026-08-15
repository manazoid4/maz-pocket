from __future__ import annotations

import struct

import pytest

from updater.flash_plan import (
    ESP_IMAGE_MAGIC,
    PARTITION_ENTRY_SIZE,
    PARTITION_MAGIC,
    PARTITION_TABLE_SIZE,
    Partition,
    PlanError,
    find_otadata_partition,
    ota_select_crc,
    parse_partition_table,
    selected_ota_partition,
    validate_firmware,
)


def part(ptype: int, subtype: int, offset: int, size: int, label: str) -> bytes:
    raw = label.encode()[:15].ljust(16, b"\0")
    return struct.pack("<HBBII16sI", PARTITION_MAGIC, ptype, subtype, offset, size, raw, 0)


def table(*entries: bytes) -> bytes:
    data = b"".join(entries) + b"\xff" * PARTITION_ENTRY_SIZE
    return data.ljust(PARTITION_TABLE_SIZE, b"\xff")


def ota(seq: int, state: int = 0xFFFFFFFF) -> bytes:
    return struct.pack("<I20sII", seq, b"\xff" * 20, state, ota_select_crc(seq))


def minimal_image(total: int = 256) -> bytes:
    header = bytearray(24)
    header[0] = ESP_IMAGE_MAGIC
    header[1] = 1
    segment = struct.pack("<II", 0x3FC80000, 16) + (b"A" * 16)
    return bytes(header) + segment + b"\0" * (total - 48)


def layout() -> list[Partition]:
    raw = table(
        part(1, 2, 0x9000, 0x5000, "nvs"),
        part(1, 0, 0xE000, 0x2000, "otadata"),
        part(0, 0x10, 0x10000, 0x200000, "MAZ-Pocket"),
        part(0, 0x11, 0x210000, 0x200000, "Bruce"),
        part(0, 0x20, 0x7A0000, 0x50000, "launcher"),
    )
    return parse_partition_table(raw)


def test_crc_matches_espressif_known_sequence() -> None:
    assert ota_select_crc(1) == 0x4743989A


def test_live_layout_and_selected_slot() -> None:
    parts = layout()
    o = bytearray(b"\xff" * 0x2000)
    o[:32] = ota(1)
    selected = selected_ota_partition(parts, bytes(o))
    assert selected.label == "MAZ-Pocket"
    assert selected.offset == 0x10000
    assert find_otadata_partition(parts).offset == 0xE000


def test_newer_valid_otadata_copy_wins() -> None:
    parts = layout()
    o = bytearray(b"\xff" * 0x2000)
    o[:32] = ota(1)
    o[0x1000 : 0x1020] = ota(2)
    assert selected_ota_partition(parts, bytes(o)).label == "Bruce"


def test_bad_crc_is_refused() -> None:
    parts = layout()
    o = bytearray(b"\xff" * 0x2000)
    bad = bytearray(ota(1))
    bad[28:32] = b"\0\0\0\0"
    o[:32] = bad
    with pytest.raises(PlanError, match="no valid"):
        selected_ota_partition(parts, bytes(o))


def test_overlap_is_refused() -> None:
    raw = table(
        part(1, 2, 0x9000, 0x5000, "nvs"),
        part(1, 0, 0xD000, 0x2000, "overlap"),
    )
    with pytest.raises(PlanError, match="overlapping"):
        parse_partition_table(raw)


def test_too_large_firmware_is_refused_before_write() -> None:
    target = Partition(0, 0x10, 0x10000, 128, "MAZ")
    with pytest.raises(PlanError, match="only 128 bytes"):
        validate_firmware(minimal_image(256), target)


def test_valid_image_is_accepted() -> None:
    target = Partition(0, 0x10, 0x10000, 0x200000, "MAZ")
    validate_firmware(minimal_image(256), target)
