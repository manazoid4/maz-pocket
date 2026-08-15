import { PARTITION_OFFSET, PARTITION_SIZE, parsePartitions, findMazPartition } from "./partition.js";

const ESPTOOL_URL = "https://unpkg.com/esptool-js@0.6.0/bundle.js";
const POLYFILL_URL = "https://unpkg.com/web-serial-polyfill@1.0.15/dist/serial.js";
const MANIFEST_URL = "./firmware/manifest.json";
const ESP_IMAGE_MAGIC = 0xe9;
const ESPRESSIF_VID = 0x303a;

const $ = (id) => document.getElementById(id);
const go = $("go");
const progress = $("progress");
const state = $("state");
const logBox = $("log");
const custom = $("custom");

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

function fail(message) {
  setStage(progress.value, message, "bad");
  log(`ERROR: ${message}`);
}

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
  return response.json();
}

async function loadFirmware() {
  const local = custom.files?.[0];
  if (local) {
    const data = new Uint8Array(await local.arrayBuffer());
    if (!validEspImage(data)) throw new Error("Selected file is not a valid ESP32 application image.");
    return { data, name: local.name, sha: await sha256(data), manifest: null };
  }

  const manifest = await loadManifest();
  const data = await fetchBytes(`./firmware/${manifest.file}`);
  if (!validEspImage(data)) throw new Error("Published v0.03 image is invalid or truncated.");
  const actual = await sha256(data);
  if (!manifest.sha256 || actual.toLowerCase() !== String(manifest.sha256).toLowerCase()) {
    throw new Error("Published firmware SHA-256 does not match its manifest.");
  }
  if (manifest.size && Number(manifest.size) !== data.byteLength) {
    throw new Error("Published firmware size does not match its manifest.");
  }
  return { data, name: manifest.file, sha: actual, manifest };
}

async function serialApi() {
  if ("serial" in navigator) return navigator.serial;
  if (!("usb" in navigator)) {
    throw new Error("This browser has neither Web Serial nor WebUSB. Use Chrome/Edge desktop or Chrome on Android.");
  }
  log("Native Web Serial unavailable; loading Android WebUSB serial compatibility layer.");
  const mod = await import(POLYFILL_URL);
  return mod.serial;
}

const terminal = {
  clean() {},
  writeLine(data) { if (data && !String(data).includes("Stub running")) log(String(data).trim()); },
  write(data) { const s = String(data || "").trim(); if (s) log(s); },
};

async function connect() {
  const api = await serialApi();
  const port = await api.requestPort({ filters: [{ usbVendorId: ESPRESSIF_VID }] });
  const esp = await import(ESPTOOL_URL);
  transport = new esp.Transport(port, true);
  loader = new esp.ESPLoader({ transport, baudrate: 460800, terminal, debugLogging: false });

  let chip;
  try {
    chip = await loader.main("default_reset");
  } catch (first) {
    log(`Automatic reset did not sync: ${first?.message || first}`);
    try {
      chip = await loader.main("no_reset");
    } catch {
      throw new Error("Could not enter ESP32 download mode. Unplug Cardputer, hold G0 (upper-right), plug USB back in, release G0, then tap CONNECT & UPDATE again.");
    }
  }
  if (!String(chip).toUpperCase().includes("ESP32-S3")) {
    throw new Error(`Connected device is ${chip || "unknown"}, not an ESP32-S3 Cardputer ADV.`);
  }
  log(`Connected: ${chip}`);
}

async function readFlash(address, size, from, to, label) {
  return loader.readFlash(address, size, (_packet, done, total) => {
    const ratio = total ? done / total : 0;
    setStage(from + (to - from) * ratio, label);
  });
}

async function writeImage(data, address, from, to, label) {
  await loader.writeFlash({
    fileArray: [{ data, address }],
    flashMode: "keep",
    flashFreq: "keep",
    flashSize: "keep",
    eraseAll: false,
    compress: true,
    reportProgress: (_index, written, total) => {
      const ratio = total ? written / total : 0;
      setStage(from + (to - from) * ratio, label);
    },
  });
}

function downloadBackup(bytes, target) {
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const blob = new Blob([bytes], { type: "application/octet-stream" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = `maz-pocket-backup-${stamp}-0x${target.offset.toString(16)}.bin`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(link.href), 5000);
}

async function loadKnownRecovery(manifest, target) {
  if (!manifest?.recovery_file || !manifest?.recovery_sha256) return null;
  const recovery = await fetchBytes(`./firmware/${manifest.recovery_file}`);
  if (!validEspImage(recovery) || recovery.byteLength > target.size) return null;
  const digest = await sha256(recovery);
  return digest.toLowerCase() === String(manifest.recovery_sha256).toLowerCase() ? recovery : null;
}

async function rollback(target, backup, knownRecovery) {
  const image = backup[0] === ESP_IMAGE_MAGIC ? backup : knownRecovery;
  if (!image) throw new Error("No bootable rollback image was available. The downloaded raw backup is still preserved on your device.");
  log("Verification failed; restoring the previous known-good Maz Pocket image.");
  setStage(70, "ROLLBACK: restoring previous Maz Pocket", "bad");
  await writeImage(image, target.offset, 70, 90, "ROLLBACK: writing recovery");
  const check = await readFlash(target.offset, image.byteLength, 90, 98, "ROLLBACK: verifying recovery");
  if (await sha256(check) !== await sha256(image)) throw new Error("Rollback write also failed verification.");
  log("Rollback verified.");
  try { await loader.after("hard_reset"); } catch {}
}

async function runUpdate() {
  if (busy) return;
  busy = true;
  go.disabled = true;
  logBox.textContent = "MAZ Pocket safe flasher started.";
  let target = null;
  let backup = null;
  let knownRecovery = null;
  let wrote = false;

  try {
    setStage(2, "Loading build metadata…");
    const firmware = await loadFirmware();
    log(`Firmware: ${firmware.name} / ${firmware.data.byteLength.toLocaleString()} bytes / ${firmware.sha.slice(0, 12)}…`);

    setStage(6, "Choose Cardputer ADV…");
    await connect();

    setStage(12, "Reading live M5Launcher partition map…");
    const table = await readFlash(PARTITION_OFFSET, PARTITION_SIZE, 12, 16, "Reading live partition map…");
    target = findMazPartition(parsePartitions(table));
    log(`MAZ partition: ${target.label} @ 0x${target.offset.toString(16)} / ${target.size.toLocaleString()} bytes`);

    if (firmware.data.byteLength > target.size) {
      throw new Error(`v0.03 is ${firmware.data.byteLength.toLocaleString()} bytes but your existing Maz slot is ${target.size.toLocaleString()} bytes. Nothing was erased.`);
    }

    setStage(18, "Backing up existing Maz Pocket…");
    backup = await readFlash(target.offset, target.size, 18, 38, "Backing up existing Maz Pocket…");
    const backupHash = await sha256(backup);
    downloadBackup(backup, target);
    log(`Backup: ${backup.byteLength.toLocaleString()} bytes / ${backupHash.slice(0, 12)}… / downloaded`);

    if (backup[0] !== ESP_IMAGE_MAGIC) {
      log("Current slot header is not bootable (likely from the failed older updater); loading known v0.02 recovery fallback.");
      knownRecovery = await loadKnownRecovery(firmware.manifest, target);
      if (knownRecovery) log("Known v0.02 recovery image verified and ready if rollback is needed.");
    }

    setStage(40, "Writing v0.03 to MAZ-Pocket only…");
    wrote = true;
    await writeImage(firmware.data, target.offset, 40, 76, "Writing v0.03 to MAZ-Pocket only…");

    setStage(77, "Reading v0.03 back for verification…");
    const verify = await readFlash(target.offset, firmware.data.byteLength, 77, 94, "Reading v0.03 back for SHA verification…");
    const actual = await sha256(verify);
    if (actual.toLowerCase() !== firmware.sha.toLowerCase()) {
      throw new Error(`Read-back SHA mismatch (${actual.slice(0, 12)}… != ${firmware.sha.slice(0, 12)}…).`);
    }
    log("Read-back SHA-256: exact match.");

    setStage(96, "Verified. Rebooting Cardputer…");
    try { await loader.after("hard_reset"); } catch (e) { log(`Reset handoff: ${e?.message || e}`); }
    setStage(100, "v0.03 FLASHED + VERIFIED", "ok");
    log("Done. Partition table, NVS, M5Launcher, SD data and sibling firmware were not written.");
  } catch (error) {
    const message = error?.message || String(error);
    fail(message);
    if (wrote && target && backup && loader) {
      try {
        await rollback(target, backup, knownRecovery);
        fail(`${message} Previous Maz Pocket was restored and verified.`);
      } catch (rollbackError) {
        fail(`${message} Rollback also needs attention: ${rollbackError?.message || rollbackError}`);
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
