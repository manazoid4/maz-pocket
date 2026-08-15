import { PARTITION_OFFSET, PARTITION_SIZE, parsePartitions, findMazPartition } from "./partition.js";
import { romReadFlashSlow } from "./rom.js";

const ESPTOOL_URL = "https://unpkg.com/esptool-js@0.6.0/bundle.js";
const POLYFILL_URL = "https://unpkg.com/web-serial-polyfill@1.0.15/dist/serial.js";
const MANIFEST_URL = "./firmware/manifest.json";
const ESP_IMAGE_MAGIC = 0xe9;
const ESPRESSIF_VID = 0x303a;
const ROM_BAUD = 115200;

const $ = (id) => document.getElementById(id);
const go = $("go");
const progress = $("progress");
const state = $("state");
const logBox = $("log");
let busy = false;
let transport = null;
let loader = null;

function log(line) {
  const now = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
  logBox.textContent += `\n${now}  ${line}`;
  logBox.scrollTop = logBox.scrollHeight;
}
function setStage(percent, text, kind = "") {
  progress.value = Math.max(0, Math.min(100, percent));
  state.textContent = text;
  state.className = kind || "dim";
}
function fail(message) { setStage(progress.value, message, "bad"); log(`ERROR: ${message}`); }
async function sha256(bytes) {
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}
function validEspImage(bytes) {
  return bytes instanceof Uint8Array && bytes.byteLength >= 64 * 1024 && bytes[0] === ESP_IMAGE_MAGIC;
}
async function fetchBytes(url) {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) throw new Error(`Could not fetch ${url} (${response.status}).`);
  return new Uint8Array(await response.arrayBuffer());
}
async function loadManifest() {
  const response = await fetch(MANIFEST_URL, { cache: "no-store" });
  if (!response.ok) throw new Error("Published firmware manifest is unavailable.");
  const m = await response.json();
  if (!m.file || !m.sha256 || !m.md5 || !m.recovery_file || !m.recovery_sha256 || !m.recovery_md5) {
    throw new Error("Firmware manifest is incomplete.");
  }
  return m;
}
async function loadAndVerify(url, expectedSha, expectedSize, label) {
  const data = await fetchBytes(url);
  if (!validEspImage(data)) throw new Error(`${label} is not a valid ESP32 application image.`);
  if (expectedSize && data.byteLength !== Number(expectedSize)) throw new Error(`${label} size does not match its manifest.`);
  if ((await sha256(data)).toLowerCase() !== String(expectedSha).toLowerCase()) throw new Error(`${label} SHA-256 does not match its manifest.`);
  return data;
}
async function serialApi() {
  if ("serial" in navigator) return navigator.serial;
  if (!("usb" in navigator)) throw new Error("Use Chrome/Edge on desktop or Chrome on Android. This browser has no Web Serial/WebUSB support.");
  log("Using Android WebUSB serial compatibility mode.");
  return (await import(POLYFILL_URL)).serial;
}

const terminal = {
  clean() {},
  writeLine(data) { const s = String(data || "").trim(); if (s) log(s); },
  write(data) { const s = String(data || "").trim(); if (s) log(s); },
};

async function detectRom(mode) {
  await loader.detectChip(mode);
  if (!loader.chip || loader.chip.CHIP_NAME !== "ESP32-S3") {
    throw new Error(`Connected device is ${loader.chip?.CHIP_NAME || "unknown"}, not a Cardputer ADV ESP32-S3.`);
  }
  if (typeof loader.chip.postConnect === "function") await loader.chip.postConnect(loader);
  loader.IS_STUB = false;
  loader.FLASH_WRITE_SIZE = loader.chip.FLASH_WRITE_SIZE || 0x400;
  const chip = await loader.chip.getChipDescription(loader);
  const flashId = await loader.readFlashId();
  if (flashId === 0xffffff || flashId === 0) throw new Error("ESP32-S3 connected, but its flash chip did not answer.");
  return chip;
}

async function connectRomOnly() {
  const api = await serialApi();
  const port = await api.requestPort({ filters: [{ usbVendorId: ESPRESSIF_VID }] });
  const esp = await import(ESPTOOL_URL);
  const makeLoader = () => {
    transport = new esp.Transport(port, true);
    loader = new esp.ESPLoader({ transport, baudrate: ROM_BAUD, terminal, debugLogging: false });
  };
  makeLoader();
  let chip;
  try {
    chip = await detectRom("default_reset");
  } catch (first) {
    log(`Automatic ROM reset did not sync: ${first?.message || first}`);
    try { await transport.disconnect(); } catch {}
    makeLoader();
    try { chip = await detectRom("no_reset"); }
    catch { throw new Error("Could not enter ESP32-S3 ROM download mode. Unplug Cardputer, hold G0 (upper-right), plug USB back in, release G0, then tap CONNECT & UPDATE again."); }
  }
  log(`ROM connected: ${chip} / no RAM flasher stub`);
}

async function readPartitionTable() {
  return romReadFlashSlow(loader, PARTITION_OFFSET, PARTITION_SIZE, (done, total) => {
    setStage(12 + (done / total) * 8, "Reading live M5Launcher partition map in ROM mode…");
  });
}

async function verifyFlashMd5(address, size, expectedMd5) {
  loader.IS_STUB = false;
  const actual = String(await loader.flashMd5sum(address, size)).toLowerCase();
  if (actual !== String(expectedMd5).toLowerCase()) {
    throw new Error(`Flash verification failed (${actual} != ${expectedMd5}).`);
  }
  return actual;
}

async function writeImage(data, address, expectedMd5, from, to, label) {
  loader.IS_STUB = false;
  loader.FLASH_WRITE_SIZE = loader.chip.FLASH_WRITE_SIZE || 0x400;
  await loader.writeFlash({
    fileArray: [{ data, address }],
    flashMode: "keep",
    flashFreq: "keep",
    flashSize: "keep",
    eraseAll: false,
    compress: true,
    reportProgress: (_index, written, total) => {
      setStage(from + (to - from) * (total ? written / total : 0), label);
    },
  });
  await verifyFlashMd5(address, data.byteLength, expectedMd5);
}

async function rollback(target, recovery, manifest) {
  log("Restoring known-good physical-hardware-accepted v0.02…");
  await writeImage(recovery, target.offset, manifest.recovery_md5, 78, 96, "ROLLBACK: restoring v0.02…");
  log("Rollback MD5 verified.");
  try { await loader.after("hard_reset"); } catch {}
}

async function runUpdate() {
  if (busy) return;
  busy = true;
  go.disabled = true;
  logBox.textContent = "MAZ Pocket ROM-safe flasher started.";
  let target = null, recovery = null, manifest = null, wrote = false;
  try {
    setStage(2, "Loading verified v0.03 + recovery metadata…");
    manifest = await loadManifest();
    const firmware = await loadAndVerify(`./firmware/${manifest.file}`, manifest.sha256, manifest.size, "v0.03 firmware");
    recovery = await loadAndVerify(`./firmware/${manifest.recovery_file}`, manifest.recovery_sha256, manifest.recovery_size, "v0.02 recovery");
    log(`v0.03: ${firmware.byteLength.toLocaleString()} bytes / SHA ${String(manifest.sha256).slice(0, 12)}…`);
    log("Known-good v0.02 recovery loaded and SHA-verified before any flash write.");

    setStage(8, "Choose Cardputer ADV…");
    await connectRomOnly();
    const table = await readPartitionTable();
    target = findMazPartition(parsePartitions(table));
    log(`MAZ partition: ${target.label} @ 0x${target.offset.toString(16)} / ${target.size.toLocaleString()} bytes`);
    if (firmware.byteLength > target.size) {
      throw new Error(`v0.03 is ${firmware.byteLength.toLocaleString()} bytes but your existing Maz slot is only ${target.size.toLocaleString()} bytes. Nothing was erased.`);
    }

    setStage(22, "Checking existing Maz slot…");
    try {
      const oldMd5 = String(await loader.flashMd5sum(target.offset, recovery.byteLength)).toLowerCase();
      if (oldMd5 === String(manifest.recovery_md5).toLowerCase()) log("Current Maz slot matches the known-good v0.02 release.");
      else log("Current Maz slot differs from pristine v0.02 (expected after the failed old updater); recovery is ready.");
    } catch (e) {
      log(`Existing-slot fingerprint unavailable: ${e?.message || e}. Recovery is ready.`);
    }

    setStage(28, "Writing v0.03 to MAZ-Pocket only…");
    wrote = true;
    await writeImage(firmware, target.offset, manifest.md5, 28, 82, "Writing v0.03 in ROM mode…");
    log("ROM flash MD5: exact match.");
    setStage(94, "Verified. Rebooting Cardputer…");
    try { await loader.after("hard_reset"); } catch (e) { log(`Reset handoff: ${e?.message || e}`); }
    setStage(100, "v0.03 FLASHED + VERIFIED", "ok");
    log("Done. No RAM stub, full erase, partition-table/NVS/Launcher/SD/sibling-app write occurred.");
  } catch (error) {
    const message = error?.message || String(error);
    fail(message);
    if (wrote && target && recovery && manifest && loader) {
      try {
        await rollback(target, recovery, manifest);
        fail(`${message} Known-good v0.02 was restored and MD5-verified automatically.`);
      } catch (rollbackError) {
        fail(`${message} Automatic recovery also needs attention: ${rollbackError?.message || rollbackError}`);
      }
    }
  } finally {
    try { if (transport) await transport.disconnect(); } catch {}
    loader = null;
    transport = null;
    busy = false;
    go.disabled = false;
  }
}

go.addEventListener("click", runUpdate);
