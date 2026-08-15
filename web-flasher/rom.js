// ROM-only primitive missing from esptool-js's public slow-read API.
// The built-in esptool-js flashMd5sum() is used for verification; this module
// only implements ESP32 ROM command 0x0E so MAZ can inspect the 4KB partition
// table without uploading the RAM flasher stub.
export const READ_FLASH_SLOW = 0x0e;
export const ROM_READ_BLOCK = 64;

function int32(value) {
  const out = new Uint8Array(4);
  new DataView(out.buffer).setUint32(0, value >>> 0, true);
  return out;
}

function concat(...arrays) {
  const total = arrays.reduce((n, a) => n + a.length, 0);
  const out = new Uint8Array(total);
  let offset = 0;
  for (const a of arrays) {
    out.set(a, offset);
    offset += a.length;
  }
  return out;
}

export function readFlashRequest(address, length) {
  if (!Number.isInteger(address) || address < 0 || address > 0xffffffff) {
    throw new Error("Invalid ROM flash read address.");
  }
  if (!Number.isInteger(length) || length < 1 || length > ROM_READ_BLOCK) {
    throw new Error("Invalid ROM flash read length.");
  }
  return concat(int32(address), int32(length));
}

export async function romReadFlashSlow(loader, address, size, onProgress = null) {
  if (!loader?.checkCommand) throw new Error("ESP ROM loader is not connected.");
  if (!Number.isInteger(size) || size < 0) throw new Error("Invalid ROM flash read size.");
  const output = new Uint8Array(size);
  let done = 0;
  while (done < size) {
    const length = Math.min(ROM_READ_BLOCK, size - done);
    const packet = readFlashRequest(address + done, length);
    const block = await loader.checkCommand(
      "read flash block",
      READ_FLASH_SLOW,
      packet,
      0,
      ROM_READ_BLOCK,
      3000,
    );
    if (!(block instanceof Uint8Array) || block.length < length) {
      throw new Error(`ROM flash read returned ${block?.length || 0} of ${length} bytes.`);
    }
    output.set(block.slice(0, length), done);
    done += length;
    if (onProgress && (done % 1024 === 0 || done === size)) onProgress(done, size);
  }
  return output;
}
