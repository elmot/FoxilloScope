# FoxilloScope – Developer & Contributor Guide

This guide provides technical information for developers building firmware, setting up hardware debug configurations, and contributing to the FoxilloScope project.

For end-user quick start instructions and assembly steps, see [README.md](README.md).

---

## 🤖 AI-Assisted Development Invitation

Feel free to use AI coding agents (such as Google Antigravity, Claude, ChatGPT, Cursor, etc.) to modify, extend, or adapt this project to your needs!

To make AI pair-programming seamless and reliable, dedicated **`AGENTS.md`** context files are placed throughout every subsystem directory:
- **Root / STM32 Firmware**: [AGENTS.md](AGENTS.md) – Covers MCU AFE scaling math, ADC phase interleaving, trigger engine, and dual UART DMA transmission.
- **ESP32 Gateway Subsystem**: [esp32-gateway/AGENTS.md](esp32-gateway/AGENTS.md) – Covers WebSocket/BLE streaming, network configuration, and AN3155 in-application STM32 firmware flashing.
- **Web Frontend Subsystem**: [html/AGENTS.md](html/AGENTS.md) – Covers uPlot integration, custom canvas drawing hooks, parameter scaling, and gesture interactions.

---

## Subsystem Architecture & Codebase Structure

The project is split into three main software components:

- **[STM32 Firmware](AGENTS.md)** (`Core/`) – Runs on the STM32G474RET6 MCU. Handles high-speed ADC sampling, hardware AFE scaling, trigger logic, and dual UART DMA transmission.
- **[ESP32 Gateway Firmware](esp32-gateway/AGENTS.md)** (`esp32-gateway/`) – ESP-IDF v6.0 project. While an M5Stamp C3U (ESP32-C3) is used by default, **any Wi-Fi/BLE-enabled ESP32 chip or board** (e.g., ESP32, ESP32-C3/C5/C6, ESP32-S2/S3) supported by ESP-IDF can be used. Refer to the official [ESP-IDF Target Hardware Documentation](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/get-started/index.html#target-hardware) for configuring alternative target chips (`idf.py set-target <chip>`).
- **[Web Frontend](html/AGENTS.md)** (`html/`) – Single-page HTML/JS application (`index.html`) using uPlot for real-time waveform rendering, gesture interactions, gain scaling, and measurement readouts.
- **Hardware Documentation** (`docs/`) – Wiring diagrams, pin maps, and schematics.
- **Python Debug Server** (`py/`) – Local desktop development bridge over USB Serial.

---

## Power & Debug Hardware Configurations

### Production Mode (Standalone Wireless)
For standard standalone production deployment and power jumper setups, see the **[Production Jumper Settings in README.md](README.md#assemble-at-home-quick-start)**:
- **Power**: Nucleo is powered directly via the M5Stamp 3.3V rail.
- **Jumper Settings**: `JP1` closed, `JP5` & `JP3` open (blocks onboard ST-LINK from interfering in off/reset state).

### Wi-Fi Debug Mode (Development)
- **Power & Wiring**: M5Stamp and Nucleo share a common ground (GND), are powered independently (e.g., separate USB cables), and have RX & TX connected.
- **Jumper Settings**:
  - **JP1**: Open
  - **JP5**: Set to **5V_STLINK**
  - **JP3**: Closed
- **Hardware Debug Interfaces**:
  - **M5Stamp C3U**: Debugged via on-chip USB-JTAG interface.
  - **Nucleo-G474RE**: Debugged via onboard ST-LINK debugger.

---

## Python Debug Server (Desktop Development)

For local web UI development without needing the ESP32 gateway hardware, run the Python debug server ([py/chart_server.py](py/chart_server.py)) on a desktop PC:

```sh
cd py
python chart_server.py
```

This bridges the STM32's USB Virtual COM port (LPUART1) directly to a local WebSocket server at `http://localhost:8000/`.

---

## Tools Used & Reference Links

- **STM32 Firmware**:
  - [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) – Graphical peripheral configuration tool
  - [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html) – Command-line build toolset and GCC ARM toolchain
  - [JetBrains CLion](https://www.jetbrains.com/clion/) – Cross-platform C/C++ IDE
- **ESP32 Gateway**:
  - [Espressif ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/) – Official development framework for ESP32 chips
- **Schematics & Wiring**:
  - [DigiKey Scheme-it](https://www.digikey.com/schemeit/project/) – Free online schematic capture tool
- **Web Frontend**:
  - Vanilla JavaScript
  - [uPlot](https://github.com/leeoniya/uPlot) – Fast, lightweight 2D canvas charting engine
  - [Lucide Icons](https://lucide.dev/) – Open-source icon library

---

## Documentation References

- **User Guide & Assembly**: [README.md](README.md)
- **STM32 Architecture Reference**: [AGENTS.md](AGENTS.md)
- **ESP32 Gateway Architecture Reference**: [esp32-gateway/AGENTS.md](esp32-gateway/AGENTS.md)
- **Frontend Architecture Reference**: [html/AGENTS.md](html/AGENTS.md)
- **Pinout & Wiring Mapping**: [docs/pinout-peripherals.md](docs/pinout-peripherals.md) & [docs/wiring.png](docs/wiring.png)
