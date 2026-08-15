// ROM-only primitives for Cardputer ADV.
// esptool-js normally uploads a RAM flasher stub. Cardputer ADV's native USB
// path has already proven more reliable with --no-stub, so MAZ stays in ROM.
export const READ_FLASH_SLOW = 0x0e;
export const ROM_READ_BLOCK = 64;

export function readFlashRequest(address, length) {
  if (!Number.isInteger(address) || address < 0 || address > 0xffffffff) {
    throw new Error("Invalid ROM flash read address.");
  }
  if (!Number.isInteger(length) || length < 1 || length > ROM_READ_BLOCK) {
    throw new Error("Invalid ROM flash read length.");
  }
  const packet = new Uint8Array(8);
  const view = new DataView(packet.buffer);
  view.setUint32(0, address >>> 0, true);
  view.setUint32(4, length >>> 0, true);
  return packet;
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
