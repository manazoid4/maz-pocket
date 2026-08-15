import { PARTITION_OFFSET, PARTITION_SIZE, parsePartitions, findMazPartition } from "./partition.js";
import { romReadFlashSlow } from "./rom.js";

const ESPTOOL_URL = "https://unpkg.com/esptool-js@0.6.0/bundle.js";
const POLYFILL_URL = "https://unpkg.com/web-serial-polyfill@1.0.15/dist/serial.js";
const FFLATE_URL = "https://unpkg.com/fflate@0.8.2/esm/browser.js";
const ESP_IMAGE_MAGIC = 0xe9;
const ESPRESSIF_VID = 0x303a;
const ROM_BAUD = 115200;

// Trust anchors for the exact CI-approved transition package. The manifest
// inside a user-selected ZIP is metadata, not authority: both firmware hashes
// are pinned here before any serial connection or write is allowed.
const EXPECTED_VERSION = "0.3.0";
const EXPECTED_FIRMWARE_SHA256 = "6fc6f90f1bb782c5a0d59a69bedc3a2ca7a85eb05aab3c1bd448310f6f8a06d0";
const EXPECTED_RECOVERY_SHA256 = "2d1eaf0495eceadf0c87d613cca4b9f2819e3ff26ed20584e5bf4d649443f9ed";
const EXPECTED_SLOT_SIZE = 0x180000;

const $ = (id) => document.getElementById(id);
const go = $("go");
const packageInput = $("package");
const packageState = $("packageState");
const progress = $("progress");
const state = $("state");
const logBox = $("log");
let busy = false;
let transport = null;
let loader = null;
let verifiedPackage = null;

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
function oneZipFile(files, suffix) {
  const matches = Object.entries(files).filter(([name]) => name.replaceAll("\\", "/").endsWith(suffix));
  if (matches.length !== 1) throw new Error(`Package must contain exactly one ${suffix}.`);
  return matches[0][1];
}

async function verifySelectedPackage(file) {
  if (!file) return;
  verifiedPackage = null;
  go.disabled = true;
  packageState.className = "dim";
  packageState.textContent = "Checking package…";
  logBox.textContent = "MAZ Pocket package verification started.";
  setStage(0, "Verifying package before USB access…");

  try {
    if (file.size < 2_000_000 || file.size > 8_000_000) throw new Error("This does not look like the MAZ Pocket release ZIP.");
    const { unzipSync } = await import(FFLATE_URL);
    const files = unzipSync(new Uint8Array(await file.arrayBuffer()));
    const manifestBytes = oneZipFile(files, "web-flasher/firmware/manifest.json");
    const firmware = oneZipFile(files, "web-flasher/firmware/maz-pocket-app.bin");
    const recovery = oneZipFile(files, "web-flasher/firmware/maz-pocket-v0.02-recovery.bin");
    const manifest = JSON.parse(new TextDecoder().decode(manifestBytes).replace(/^\uFEFF/, ""));

    if (manifest.product !== "MAZ Pocket" || manifest.version !== EXPECTED_VERSION) throw new Error("Package is not the approved MAZ Pocket v0.03 release.");
    if (!validEspImage(firmware) || !validEspImage(recovery)) throw new Error("Package contains an invalid ESP32 application image.");
    if (Number(manifest.size) !== firmware.byteLength || Number(manifest.recovery_size) !== recovery.byteLength) throw new Error("Package image sizes do not match its manifest.");
    if (Number(manifest.launcher_slot_size) !== EXPECTED_SLOT_SIZE || firmware.byteLength > EXPECTED_SLOT_SIZE) throw new Error("Package does not match the physical v0.02 Launcher slot contract.");

    const firmwareSha = await sha256(firmware);
    const recoverySha = await sha256(recovery);
    if (firmwareSha !== EXPECTED_FIRMWARE_SHA256 || String(manifest.sha256).toLowerCase() !== EXPECTED_FIRMWARE_SHA256) throw new Error("v0.03 firmware SHA-256 failed. Nothing can be flashed.");
    if (recoverySha !== EXPECTED_RECOVERY_SHA256 || String(manifest.recovery_sha256).toLowerCase() !== EXPECTED_RECOVERY_SHA256) throw new Error("v0.02 recovery SHA-256 failed. Nothing can be flashed.");
    if (!/^[0-9a-f]{32}$/i.test(manifest.md5) || !/^[0-9a-f]{32}$/i.test(manifest.recovery_md5)) throw new Error("Package flash-verification metadata is invalid.");

    verifiedPackage = { manifest, firmware, recovery };
    packageState.textContent = `VERIFIED • v0.03 • ${firmware.byteLength.toLocaleString()} bytes`;
    packageState.className = "ok";
    setStage(0, "Package verified. Ready to connect Cardputer.", "ok");
    log(`Package SHA verified: ${firmwareSha.slice(0, 16)}…`);
    log(`Recovery SHA verified: ${recoverySha.slice(0, 16)}…`);
    go.disabled = false;
  } catch (error) {
    const message = error?.message || String(error);
    packageState.textContent = `REJECTED • ${message}`;
    packageState.className = "bad";
    fail(message);
  }
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
    transport = new esp.Transport(port, false);
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
  if (busy || !verifiedPackage) return;
  busy = true;
  go.disabled = true;
  logBox.textContent = "MAZ Pocket ROM-safe flasher started.";
  const { manifest, firmware, recovery } = verifiedPackage;
  let target = null, wrote = false;
  try {
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
      else log("Current Maz slot differs from pristine v0.02 (possible after the failed old updater); recovery is ready.");
    } catch (e) {
      log(`Existing-slot fingerprint unavailable: ${e?.message || e}. Recovery remains ready.`);
    }

    setStage(28, "Writing v0.03 to Maz Pocket only…");
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
    go.disabled = !verifiedPackage;
  }
}

packageInput.addEventListener("change", () => verifySelectedPackage(packageInput.files?.[0]));
go.addEventListener("click", runUpdate);
