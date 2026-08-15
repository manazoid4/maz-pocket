// ROM-only primitives for Cardputer ADV.
// esptool-js normally uploads a RAM flasher stub. Cardputer ADV's native USB
// path has already proven more reliable with --no-stub, so MAZ stays in ROM.
export const READ_FLASH_SLOW = 0x0e;
export const SPI_FLASH_MD5 = 0x13;
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

export function flashMd5Request(address, size) {
  if (!Number.isInteger(address) || address < 0 || address > 0xffffffff) {
    throw new Error("Invalid ROM MD5 address.");
  }
  if (!Number.isInteger(size) || size < 1 || address + size > 0x100000000) {
    throw new Error("Invalid ROM MD5 size.");
  }
  return concat(int32(address), int32(size), int32(0), int32(0));
}

export function normalizeRomMd5(data) {
  if (!(data instanceof Uint8Array)) throw new Error("ROM flash MD5 returned no data.");
  const text = new TextDecoder().decode(data).replace(/\0/g, "").trim().toLowerCase();
  if (/^[0-9a-f]{32}$/.test(text)) return text;
  if (data.length === 16) {
    return [...data].map((b) => b.toString(16).padStart(2, "0")).join("");
  }
  throw new Error("ROM flash MD5 response was malformed.");
}

export async function romFlashMd5(loader, address, size) {
  if (!loader?.checkCommand) throw new Error("ESP ROM loader is not connected.");
  const packet = flashMd5Request(address, size);
  const timeout = Math.max(3000, Math.ceil((size / 1000000) * 10000));
  const data = await loader.checkCommand(
    "calculate flash MD5",
    SPI_FLASH_MD5,
    packet,
    0,
    32,
    timeout,
  );
  return normalizeRomMd5(data);
}

export async function romReadFlashSlow(loader, address, size, onProgress = null) {
  if (!loader?.checkCommand) throw new Error("ESP ROM loader is not connected.");
  if (!Number.isInteger(size) || size < 0) throw new Error("Invalid ROM flash read size.");
  const output = new Uint8Array(size);
  let done = 0;
  while (done < size) {
    const length = Math.min(ROM_READ_BLOCK, size - done);
    const packet = readFlashRequest(address + done, length);
    // ESP32-family ROM command 0x0E always returns a 64-byte data buffer,
    // followed by status bytes. checkCommand strips the status for us.
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
