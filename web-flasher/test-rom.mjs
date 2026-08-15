import assert from "node:assert/strict";
import {
  READ_FLASH_SLOW,
  SPI_FLASH_MD5,
  ROM_READ_BLOCK,
  readFlashRequest,
  flashMd5Request,
  normalizeRomMd5,
  romFlashMd5,
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
  const packet = flashMd5Request(0x10000, 0x180000);
  const view = new DataView(packet.buffer);
  assert.equal(packet.length, 16);
  assert.equal(view.getUint32(0, true), 0x10000);
  assert.equal(view.getUint32(4, true), 0x180000);
  assert.equal(view.getUint32(8, true), 0);
  assert.equal(view.getUint32(12, true), 0);
}

{
  const ascii = new TextEncoder().encode("0123456789abcdef0123456789abcdef");
  assert.equal(normalizeRomMd5(ascii), "0123456789abcdef0123456789abcdef");
  const raw = Uint8Array.from({ length: 16 }, (_, i) => i);
  assert.equal(normalizeRomMd5(raw), "000102030405060708090a0b0c0d0e0f");
  assert.throws(() => normalizeRomMd5(new Uint8Array(8)), /malformed/);
}

{
  const calls = [];
  const loader = {
    async checkCommand(description, opcode, packet, checksum, responseLength) {
      const view = new DataView(packet.buffer, packet.byteOffset, packet.byteLength);
      const address = view.getUint32(0, true);
      const requested = view.getUint32(4, true);
      calls.push({ description, opcode, address, requested, checksum, responseLength });
      if (opcode === SPI_FLASH_MD5) {
        return new TextEncoder().encode("abcdefabcdefabcdefabcdefabcdefab");
      }
      const block = new Uint8Array(ROM_READ_BLOCK);
      for (let i = 0; i < block.length; i++) block[i] = (address + i) & 0xff;
      return block;
    },
  };
  const progress = [];
  const data = await romReadFlashSlow(loader, 0x8000, 130, (done, total) => progress.push([done, total]));
  assert.equal(data.length, 130);
  const readCalls = calls.filter((c) => c.opcode === READ_FLASH_SLOW);
  assert.equal(readCalls.length, 3);
  assert.deepEqual(readCalls.map((c) => c.address), [0x8000, 0x8040, 0x8080]);
  assert.deepEqual(readCalls.map((c) => c.requested), [64, 64, 2]);
  assert.ok(readCalls.every((c) => c.responseLength === 64));
  assert.equal(data[0], 0x00);
  assert.equal(data[64], 0x40);
  assert.equal(data[128], 0x80);
  assert.deepEqual(progress.at(-1), [130, 130]);

  const md5 = await romFlashMd5(loader, 0x10000, 123456);
  assert.equal(md5, "abcdefabcdefabcdefabcdefabcdefab");
  const md5Call = calls.find((c) => c.opcode === SPI_FLASH_MD5);
  assert.equal(md5Call.address, 0x10000);
  assert.equal(md5Call.requested, 123456);
  assert.equal(md5Call.responseLength, 32);
}

console.log("ROM no-stub tests: PASS");
