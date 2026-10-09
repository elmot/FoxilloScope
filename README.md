# FoxilloScope (STM32G474 Oscilloscope)

An accessible, wireless oscilloscope built around low-cost development hardware: the **ST Nucleo-G474RE** board and an **M5Stamp C3U** (ESP32-C3) module.

 - Bipolar signal measurement
 - Dual-channel
 - Up to 8 MHz sampling rate
 - 12-bit resolution
 - The user interface runs in a web browser on desktop and mobile devices
 - Communication via Wi-Fi or Bluetooth Low Energy (BLE). Bluetooth requires a compatible browser (e.g., Google Chrome).
 - No installable application required
 
The goal of this project is to turn inexpensive, readily available development components into a practical, portable oscilloscope with live-streaming over Wi-Fi or Bluetooth LE — requiring minimal soldering and no custom PCBs.

> 💡 **Developer Note**: For firmware compilation, hardware debug configurations, jumper setups, and local debug servers, see the **[Developer & Contributor Guide](DEVELOPMENT.md)**.

---

## Key Features & Hardware Design

- **Affordable Hardware**: Total hardware cost is under €25 / $30 (minimal soldering, no custom PCBs needed)
  - Analog front-end (AFE) built on the ST [Nucleo-G474RE](https://www.st.com/en/evaluation-tools/nucleo-g474re.html) board – handling signal capture and triggering
  - [M5Stamp C3U](https://www.espressif.com/en/news/M5Stamp_C3U) handles wireless connectivity and hosts the web server
  - Signal conditioning built with six Schottky diodes (BAS40-04 dual-diodes or equivalent) and five resistors
  - Inexpensive oscilloscope probes (not included in BOM)
- **Bipolar Signal Measurement**: On-chip analog front-end (AFE) supports true bipolar AC/DC signal measurements (both positive and negative voltages) around virtual ground.
  > ⚠️ **Important Grounding Notice**: Users **must not** connect probe ground clips to the board's GND pins. Always connect probe ground clips to the board's dedicated **Virtual Ground** output.
  > This provides a clean mid-rail reference for bipolar measurements and prevents shorting power rails.
- **Input Voltage Range**: Measurable range −1.65 V to +1.65 V (1:10 attenuating oscilloscope probes recommended); safe input range −30 V to +30 V.
- **Input Signal Protection**: A resistor and diode protection network protects MCU inputs against out-of-range signal spikes.
- **Multi-Transport Web UI**: Single-page web dashboard accessible via Wi-Fi (WebSockets), Bluetooth LE (WebBluetooth), or USB (WebSerial).

---

## Quick Start (Flashing & Operation)

### Hardware Setup & Assembly

All hardware documentation, board modification guides, jumper configurations, and soldering diagnostics have been consolidated into the **[Hardware Guide (docs/HARDWARE.md)](docs/HARDWARE.md)**:

1. **[Board Modding First](docs/HARDWARE.md#1-nucleo-g474re-board-modifications-mandatory)** 
   1. Solder bridges (`SB17`/`SB23`, `SB18`/`SB22`, `JP8`, `JP6`).
   2. [Wiring Verification (`pin_check`)](docs/HARDWARE.md#4-hardware-verification-with-pin_check) — Diagnostic test to check proper wiring.
   3. Soldering protection network
2. **[Jumper Configuration](docs/HARDWARE.md#2-jumper-configurations)** — Jumper settings for Production and Development modes.
3. **[ESP32 Gateway Module](docs/HARDWARE.md#3-esp32-wireless-gateway-module-m5stamp-c3u)** — Power, UART connections, and RGB status LED table.


4. **Flash the STM32 Firmware**:
   1. Connect the Nucleo board to your computer via USB (in development jumper mode). A virtual USB drive will appear.
   2. Copy `FoxilloScope-Production.bin` from the latest release assets onto the virtual drive.
   3. Configure jumpers for [Production mode](docs/HARDWARE.md#2-jumper-configurations) for standalone use.
5. **Flash the ESP32 Gateway (M5Stamp C3U)**:
   1. Connect the M5Stamp C3U to your computer via USB.
   2. Use the [Foxilloscope ESP32 Web Flasher](https://elmot.xyz/f-scope/flahser.html) to upload the gateway firmware.
6. **Connect Probes**:
   1. Attach oscilloscope probes to Channel A / Channel B inputs.
   2. Connect probe ground clips to **Virtual Ground (VGND)** (never to board GND!).
7. **Connect & Measure**:
   1. **Wi-Fi**: Connect to *FoxilloScope-xxxx* Wi-Fi network and open `http://f-scope.local/` (or `http://192.168.4.1/`).
   2. **Bluetooth LE**: Open `http://f-scope.local/` in Chrome, click the Bluetooth icon, and pair with *F-SCOPE-xxxx*.
   3. **Wi-Fi Credentials Setup**: Open `http://f-scope.local/wifi` to configure your home Wi-Fi network.
   4. **Device Label** *(optional)*: Open [Sticker Generator](https://elmot.xyz/f-scope/sticker/) and enter the device ID (`xxxx`, last four hex digits of the ESP32 MAC).

---

## Deployment & Web Access

### 1. Wireless Gateway Mode (Default)
Connect your computer, phone, or tablet to the ESP32 gateway (either directly via its `FoxilloScope` AP or through your local Wi-Fi network) and open:
```text
http://f-scope.local/
```
The ESP32 serves the web frontend and streams data in real time via WebSockets.

### 2. Standalone CDN / GitHub Pages Mode
The web app can also be hosted on any static HTTPS server (e.g. GitHub Pages). In this mode, the browser connects directly to the hardware via:
- **WebBluetooth**: Connect wirelessly over BLE.
- **WebSerial**: Connect directly over USB at 460,800 baud.

---

## Documentation & References

- **[Hardware Setup & Assembly Guide](docs/HARDWARE.md)** – Board solder bridge modifications, jumpers, external circuit, and `pin_check` verification.
- **[Developer & Contributor Guide](DEVELOPMENT.md)** – Firmware compilation, debug hardware jumper configurations, Python debug server, and tools used.
- **Physical Wiring Diagram**: [docs/wiring_diagram.html](docs/wiring_diagram.html)
- **Subsystem Architecture References**:
  - STM32 Firmware Architecture: [AGENTS.md](AGENTS.md)
  - ESP32 Gateway Architecture: [esp32-gateway/AGENTS.md](esp32-gateway/AGENTS.md)
  - Web Frontend Architecture: [html/AGENTS.md](html/AGENTS.md)
