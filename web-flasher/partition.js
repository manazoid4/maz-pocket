export const PARTITION_OFFSET = 0x8000;
export const PARTITION_SIZE = 0x1000;
export const FLASH_SIZE = 8 * 1024 * 1024;
export const MAZ_LABEL = "maz-pocket";

function u32(view, offset) {
  return view.getUint32(offset, true);
}

export function parsePartitions(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.byteLength < PARTITION_SIZE) {
    throw new Error("Partition table read was incomplete.");
  }

  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const decoder = new TextDecoder();
  const parts = [];

  for (let pos = 0; pos + 32 <= PARTITION_SIZE; pos += 32) {
    const magic = view.getUint16(pos, true);
    if (magic === 0xffff || magic === 0xebeb) break;
    if (magic !== 0x50aa) {
      throw new Error(`Invalid partition entry at 0x${pos.toString(16)}.`);
    }

    const type = bytes[pos + 2];
    const subtype = bytes[pos + 3];
    const offset = u32(view, pos + 4);
    const size = u32(view, pos + 8);
    let labelBytes = bytes.slice(pos + 12, pos + 28);
    const zero = labelBytes.indexOf(0);
    if (zero >= 0) labelBytes = labelBytes.slice(0, zero);
    const label = decoder.decode(labelBytes);

    if (!size || offset < 0x1000 || offset + size > FLASH_SIZE) {
      throw new Error(`Unsafe partition bounds for ${label || "unnamed partition"}.`);
    }

    parts.push({ type, subtype, offset, size, label });
  }

  if (!parts.length) throw new Error("No valid flash partitions were found.");

  const ordered = [...parts].sort((a, b) => a.offset - b.offset);
  for (let i = 1; i < ordered.length; i++) {
    if (ordered[i].offset < ordered[i - 1].offset + ordered[i - 1].size) {
      throw new Error("The live partition table contains overlapping entries; refusing to write.");
    }
  }

  return parts;
}

export function findMazPartition(parts) {
  const matches = parts.filter((p) =>
    p.type === 0 &&
    p.subtype >= 0x10 && p.subtype < 0x20 &&
    p.label.trim().toLowerCase().startsWith(MAZ_LABEL)
  );

  if (matches.length !== 1) {
    throw new Error(matches.length === 0
      ? "No existing MAZ-Pocket Launcher partition was found. Install through M5Launcher first."
      : "More than one MAZ-Pocket partition exists; automatic in-place update is intentionally disabled.");
  }

  return matches[0];
}
