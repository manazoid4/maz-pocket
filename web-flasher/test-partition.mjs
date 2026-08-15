import assert from "node:assert/strict";
import { PARTITION_SIZE, parsePartitions, findMazPartition } from "./partition.js";

function entry({ type, subtype, offset, size, label }) {
  const out = new Uint8Array(32);
  const view = new DataView(out.buffer);
  view.setUint16(0, 0x50aa, true);
  out[2] = type;
  out[3] = subtype;
  view.setUint32(4, offset, true);
  view.setUint32(8, size, true);
  new TextEncoder().encodeInto(label.slice(0, 15), out.subarray(12, 28));
  return out;
}

function table(entries) {
  const out = new Uint8Array(PARTITION_SIZE).fill(0xff);
  let pos = 0;
  for (const item of entries) {
    out.set(entry(item), pos);
    pos += 32;
  }
  return out;
}

const base = [
  { type: 1, subtype: 2, offset: 0x9000, size: 0x5000, label: "nvs" },
  { type: 1, subtype: 0, offset: 0xe000, size: 0x2000, label: "otadata" },
  { type: 0, subtype: 0x10, offset: 0x10000, size: 0x180000, label: "MAZ-Pocket" },
  { type: 0, subtype: 0x11, offset: 0x190000, size: 0x100000, label: "Bruce" },
  { type: 0, subtype: 0x12, offset: 0x290000, size: 0x90000, label: "Nemo" },
];

{
  const parts = parsePartitions(table(base));
  const maz = findMazPartition(parts);
  assert.equal(maz.label, "MAZ-Pocket");
  assert.equal(maz.offset, 0x10000);
  assert.equal(maz.size, 0x180000);
  assert.equal(parts.find((p) => p.label === "Bruce").offset, 0x190000);
}

{
  const lower = base.map((p) => p.label === "MAZ-Pocket" ? { ...p, label: "maz-pocket-v02" } : p);
  assert.equal(findMazPartition(parsePartitions(table(lower))).offset, 0x10000);
}

{
  const none = base.map((p) => p.label === "MAZ-Pocket" ? { ...p, label: "Other" } : p);
  assert.throws(() => findMazPartition(parsePartitions(table(none))), /No existing MAZ-Pocket/);
}

{
  const duplicate = [...base, { type: 0, subtype: 0x13, offset: 0x320000, size: 0x80000, label: "MAZ-Pocket-2" }];
  assert.throws(() => findMazPartition(parsePartitions(table(duplicate))), /More than one MAZ-Pocket/);
}

{
  const wrongType = base.map((p) => p.label === "MAZ-Pocket" ? { ...p, type: 1 } : p);
  assert.throws(() => findMazPartition(parsePartitions(table(wrongType))), /No existing MAZ-Pocket/);
}

{
  const overlap = base.map((p) => p.label === "Bruce" ? { ...p, offset: 0x180000 } : p);
  assert.throws(() => parsePartitions(table(overlap)), /overlapping entries/);
}

{
  const outOfFlash = [{ type: 0, subtype: 0x10, offset: 0x7f0000, size: 0x20000, label: "MAZ-Pocket" }];
  assert.throws(() => parsePartitions(table(outOfFlash)), /Unsafe partition bounds/);
}

{
  const truncated = new Uint8Array(128);
  assert.throws(() => parsePartitions(truncated), /incomplete/);
}

console.log("partition ownership tests: PASS");
