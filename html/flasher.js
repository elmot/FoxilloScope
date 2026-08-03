// FoxilloScope ESP32-C3 Web Flasher App Logic
import { ESPLoader, Transport } from 'https://cdn.jsdelivr.net/npm/esptool-js@0.6.0/+esm';

// Local Firmware Path & Flash Address
const LOCAL_FIRMWARE_PATH = './esp32-gateway.bin';
const DEFAULT_FLASH_ADDRESS = 0x0000;

// State Variables
let firmwareBinary = null; // Uint8Array
let isFlashing = false;

// DOM Elements
const browserWarning = document.getElementById('browser-warning');
const fwNameEl = document.getElementById('fw-name');
const fwStatusTextEl = document.getElementById('fw-status-text');
const fwSizeEl = document.getElementById('fw-size');

const baudRateSelect = document.getElementById('baud-rate');
const eraseFlashCheckbox = document.getElementById('erase-flash');
const btnConnectFlash = document.getElementById('btn-connect-flash');

const progressContainer = document.getElementById('progress-container');
const progressStepText = document.getElementById('progress-step-text');
const progressPercent = document.getElementById('progress-percent');
const progressBar = document.getElementById('progress-bar');
const progressSubtext = document.getElementById('progress-subtext');

const terminal = document.getElementById('terminal');
const btnClearTerm = document.getElementById('btn-clear-term');

// Terminal Logging Helper
function logToTerminal(message, type = 'info') {
  const entry = document.createElement('div');
  entry.className = `log-entry ${type}`;
  const timestamp = new Date().toLocaleTimeString('en-US', { hour12: false });
  entry.textContent = `[${timestamp}] ${message}`;
  terminal.appendChild(entry);
  terminal.scrollTop = terminal.scrollHeight;
}

// Check Web Serial Support
function checkWebSerialSupport() {
  if (!('serial' in navigator)) {
    browserWarning.classList.remove('hidden');
    btnConnectFlash.disabled = true;
    logToTerminal('Web Serial API is not supported in this browser. Please use Chrome, Edge, or Brave on desktop.', 'error');
    return false;
  }
  return true;
}

// Load Firmware from Local Directory (esp32-gateway.bin)
async function loadLocalFirmware() {
  fwStatusTextEl.className = 'status-tag downloading';
  fwStatusTextEl.textContent = 'Loading...';
  fwSizeEl.textContent = '-- KB';

  logToTerminal(`Loading local firmware file: ${LOCAL_FIRMWARE_PATH}...`, 'info');

  try {
    const response = await fetch(LOCAL_FIRMWARE_PATH);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }

    const buffer = await response.arrayBuffer();
    firmwareBinary = new Uint8Array(buffer);
    const kbSize = (firmwareBinary.length / 1024).toFixed(1);

    fwStatusTextEl.className = 'status-tag ready';
    fwStatusTextEl.textContent = 'Loaded';
    fwSizeEl.textContent = `${kbSize} KB`;

    logToTerminal(`Firmware loaded successfully (${kbSize} KB).`, 'success');

    if (checkWebSerialSupport()) {
      btnConnectFlash.disabled = false;
    }
  } catch (err) {
    firmwareBinary = null;
    fwStatusTextEl.className = 'status-tag error';
    fwStatusTextEl.textContent = 'Failed';
    btnConnectFlash.disabled = true;
    logToTerminal(`Failed to load local firmware file (${LOCAL_FIRMWARE_PATH}): ${err.message}`, 'error');
    logToTerminal('Ensure esp32-gateway.bin is placed in the same folder as index.html.', 'warn');
  }
}

// Validate Chip Family (Must be ESP32-C3)
function isESP32C3(chipName) {
  if (!chipName) return false;
  const normalized = String(chipName).toUpperCase();
  return normalized.includes('ESP32-C3') || normalized.includes('ESP32C3');
}

// Terminal interface for esptool-js
const esptoolTerminal = {
  clean() {},
  writeLine(data) {
    if (data && data.trim()) {
      logToTerminal(data.trim(), 'info');
    }
  },
  write(data) {
    if (data && data.trim()) {
      logToTerminal(data.trim(), 'info');
    }
  }
};

// Unified Connect & Flash Flow
async function connectAndFlash() {
  if (isFlashing) return;
  if (!firmwareBinary) {
    logToTerminal('No firmware loaded! Cannot proceed.', 'error');
    return;
  }

  isFlashing = true;
  btnConnectFlash.disabled = true;
  baudRateSelect.disabled = true;
  eraseFlashCheckbox.disabled = true;

  let serialPort = null;
  let transport = null;
  let esploader = null;

  try {
    logToTerminal('Requesting serial port connection...', 'info');
    serialPort = await navigator.serial.requestPort();

    const baudRate = parseInt(baudRateSelect.value, 10);
    logToTerminal(`Opening serial port at ${baudRate} baud...`, 'info');

    transport = new Transport(serialPort, false);
    esploader = new ESPLoader({
      transport: transport,
      baudrate: baudRate,
      terminal: esptoolTerminal
    });

    logToTerminal('Connecting to ESP chip stub...', 'info');
    const detectedChip = await esploader.main();
    logToTerminal(`Chip detected: ${detectedChip}`, 'success');

    // Hardware Validation Step: ESP32-C3 Only!
    const chipNameStr = String(esploader.chip ? (esploader.chip.CHIP_NAME || detectedChip) : detectedChip);
    if (!isESP32C3(chipNameStr)) {
      throw new Error(`Incompatible chip detected: '${chipNameStr}'. ESP32-C3 is required!`);
    }

    logToTerminal('Hardware Validation PASSED: Connected device is an ESP32-C3.', 'success');

    // Prepare Progress UI
    progressContainer.classList.remove('hidden');
    progressStepText.textContent = 'Writing Flash...';
    progressPercent.textContent = '0%';
    progressBar.style.width = '0%';
    progressSubtext.textContent = `0 / ${(firmwareBinary.length / 1024).toFixed(0)} KB`;

    const flashOptions = {
      fileArray: [
        {
          data: firmwareBinary,
          address: DEFAULT_FLASH_ADDRESS
        }
      ],
      flashSize: 'keep',
      eraseAll: eraseFlashCheckbox.checked,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const pct = Math.floor((written / total) * 100);
        progressBar.style.width = `${pct}%`;
        progressPercent.textContent = `${pct}%`;
        progressSubtext.textContent = `${(written / 1024).toFixed(0)} / ${(total / 1024).toFixed(0)} KB`;
      }
    };

    if (eraseFlashCheckbox.checked) {
      logToTerminal('Erasing entire flash memory...', 'warn');
      progressStepText.textContent = 'Erasing Flash...';
    }

    logToTerminal(`Flashing esp32-gateway.bin to address 0x${DEFAULT_FLASH_ADDRESS.toString(16)}...`, 'info');
    progressStepText.textContent = 'Flashing...';

    await esploader.writeFlash(flashOptions);

    progressBar.style.width = '100%';
    progressPercent.textContent = '100%';
    progressStepText.textContent = 'Flash Complete!';
    logToTerminal('Firmware programmed successfully!', 'success');

    // Reset Chip
    logToTerminal('Performing hard reset to boot gateway firmware...', 'info');
    await esploader.after('hard_reset');
    logToTerminal('Hard reset complete. FoxilloScope gateway is active!', 'success');

  } catch (err) {
    progressStepText.textContent = 'Flash Failed';
    logToTerminal(`Operation failed: ${err.message}`, 'error');
  } finally {
    isFlashing = false;
    btnConnectFlash.disabled = false;
    baudRateSelect.disabled = false;
    eraseFlashCheckbox.disabled = false;

    // Disconnect transport
    try {
      if (transport) await transport.disconnect();
      if (serialPort) await serialPort.close();
    } catch (e) {
      // Ignore cleanup errors
    }
  }
}

// Initialize Application
document.addEventListener('DOMContentLoaded', () => {
  logToTerminal('Initializing FoxilloScope ESP32-C3 Online Flasher...', 'system');
  checkWebSerialSupport();

  fetch('./version.txt').then(r => r.ok && r.text()).then(v => { if (v) document.getElementById('fw-version').textContent = v.trim(); });

  btnConnectFlash.addEventListener('click', connectAndFlash);
  btnClearTerm.addEventListener('click', () => {
    terminal.innerHTML = '';
    logToTerminal('Console log cleared.', 'system');
  });

  loadLocalFirmware();
});
