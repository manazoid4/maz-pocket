import assert from "node:assert/strict";
import {
  READ_FLASH_SLOW,
  ROM_READ_BLOCK,
  readFlashRequest,
  romReadFlashSlow,
} from "./rom.js";

{
  const packet = readFlashRequest(0x123456, 64);
  const view = new DataView(packet.buffer);
  assert.equal(packet.length, 8);
  assert.equal(view.getUint32(0, true), 0x123456);
  assert.equal(view.getUint32(4, true), 64);
}
assert.throws(() => readFlashRequest(-1, 64), /address/);
assert.throws(() => readFlashRequest(0, 65), /length/);

{
  const calls = [];
  const loader = {
    async checkCommand(description, opcode, packet, checksum, responseLength) {
      const view = new DataView(packet.buffer, packet.byteOffset, packet.byteLength);
      const address = view.getUint32(0, true);
      const requested = view.getUint32(4, true);
      calls.push({ description, opcode, address, requested, checksum, responseLength });
      const block = new Uint8Array(ROM_READ_BLOCK);
      for (let i = 0; i < block.length; i++) block[i] = (address + i) & 0xff;
      return block;
    },
  };
  const progress = [];
  const data = await romReadFlashSlow(loader, 0x8000, 130, (done, total) => progress.push([done, total]));
  assert.equal(data.length, 130);
  assert.equal(calls.length, 3);
  assert.deepEqual(calls.map((c) => c.opcode), [READ_FLASH_SLOW, READ_FLASH_SLOW, READ_FLASH_SLOW]);
  assert.deepEqual(calls.map((c) => c.address), [0x8000, 0x8040, 0x8080]);
  assert.deepEqual(calls.map((c) => c.requested), [64, 64, 2]);
  assert.ok(calls.every((c) => c.responseLength === 64));
  assert.equal(data[0], 0x00);
  assert.equal(data[64], 0x40);
  assert.equal(data[128], 0x80);
  assert.deepEqual(progress.at(-1), [130, 130]);
}

console.log("ROM no-stub partition-read tests: PASS");
