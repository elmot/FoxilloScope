# FoxilloScope — Hardware Setup & Assembly Guide

This guide covers all hardware aspects of building and verifying the FoxilloScope: modifying the ST Nucleo-G474RE board, configuring jumpers for development and production, wiring the ESP32 wireless module, assembling the external analog front-end (AFE) circuit, and running automated hardware verification via `pin_check`.

---

## 1. Nucleo-G474RE Board Modifications (Mandatory)

All solder bridges (`SB1`–`SB41`) are located on the **bottom layer** of the ST Nucleo-G474RE board ([MB1367](mb1367-g474re-c04_schematic.pdf)):

- **Desolder / Open SB17 & SB23**: Disconnects `LPUART1` (`PA2`/`PA3`) from ST-LINK Virtual COM Port.
- **Solder Bridge / Close SB18 & SB22**: Routes `LPUART1` (`PA2`/`PA3`) to expansion headers (D1 TX / D0 RX) for ESP32 UART communication.
- **Solder Bridge JP8 to pins 2–3 (VDD)**: Ties MCU $V_{\text{REF+}}$ analog reference directly to 3.3 V $V_{\text{DD}}$ rail for accurate ADC/DAC scaling math.
- **Solder Bridge JP6 across pins 1–2**: Bypasses $I_{\text{DD}}$ measurement header contacts for reliability.
- *(Optional for ST-LINK debug console)*: Desolder **SB13** & **SB19**, close **SB12** & **SB20** to route `USART1` (`PC4`/`PC5`) to ST-LINK VCP instead.

---

## 2. Jumper Configurations

The Nucleo-G474RE board power and boot jumpers must be configured according to your operating mode:

| Mode | JP5 (Power Source) | JP1 (ST-LINK Reset) | JP3 (ST-LINK 5V) | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Development ** | **5V_STLK** | **Open** | **Closed** | Normal debugging and firmware flashing via onboard ST-LINK. Both Nucleo ST-LINK USB and ESP32 USB can be connected simultaneously. |
| **Production ** | **E5V** | **Closed** | **Open** | Standalone wireless operation. JP1 held closed holds the unpowered ST-LINK MCU in reset, preventing phantom power draw and bus loading. Board is powered from ESP32 / external 5V regulator. |

---

## 3. ESP32 Wireless Gateway Module (M5Stamp C3U)

The M5Stamp C3U acts as the wireless bridge connecting the STM32 to your browser over Wi-Fi (WebSockets) or Bluetooth Low Energy (BLE).

### Pin Connections Between Nucleo & ESP32
- **Power**: Connect 5V and GND between Nucleo and M5Stamp C3U.
- **ESP32 GPIO 6 (TX)** $\rightarrow$ **STM32 UART RX** (`PA3` / D0)
- **ESP32 GPIO 7 (RX)** $\rightarrow$ **STM32 UART TX** (`PA2` / D1)
- **ESP32 GPIO 2**: Data line for onboard SK6812 RGB LED (WS2812 protocol)

### Onboard RGB LED Statuses

| LED Color         | Status / Meaning                                                      |
|:------------------|:----------------------------------------------------------------------|
| **Green**         | Client actively connected over **WebSocket**                          |
| **Blue**          | Client actively connected over **Bluetooth LE (BLE)**                 |
| **Orange**        | Wi-Fi station connection failed (fallback Access Point is active)     |
| **White**         | Connected to Wi-Fi network, waiting for web client                    |
| **Purple**        | Idle in Access Point mode (`FoxilloScope` AP), waiting for connection |
| **Blinking Pink** | STM32 firmware update complete (power cycle required)                 |

---

## 4. Hardware Verification with `pin_check`

> ⚠️ **IMPORTANT: Perform the pin check BEFORE soldering resistors, diodes, or other external components onto the board!**
> Diodes and pull resistors will distort high-impedance pull-up/pull-down tests and can trigger false positive shorts or leaks. The test must be run on bare interconnections and solder bridges first.

Before soldering the analog front-end components or powering up the full oscilloscope firmware, flash the diagnostic tool **`pin_check`** to verify all solder bridges, jumper wires, and header connections without risking damage to the analog peripherals.

### What `pin_check` Does
1. **Power Rail Fault Detection**: Tests every pin for accidental shorts to **GND** or **VDD** (3.3 V).
2. **Net Continuity Verification**: Verifies bidirectional continuity across all multi-pin nets (both internal MCU-to-MCU links like `_STAGE.A`, `_BIAS.A` and external shared nets like `VGND`).
3. **Net Isolation (Short-Circuit) Check**: Tests all pin-to-pin combinations to guarantee no unintended bridges exist between independent circuits.
4. **Machine-Readable Report**: Automatically outputs `pin_report.json` via Arm semihosting and prints diagnostic summaries to the console.
5. **Interactive LED Signal Generator (Visual Verification)**:
   - After the report finishes, `pin_check` switches all external pins (`VGND`, `IN.A`, `IN.B`, `TEST.SIGNAL`) to push-pull output mode.
   - It outputs distinct duty cycle pulses (e.g. 20%, 40%, 60%, 80% on a 1-second cycle) grouped by net.
   - Pins sharing the same net blink in unison (no contention).
   - You can connect an LED (with a current-limiting series resistor, e.g. $1\text{ k}\Omega$) between any external pin and GND to visually verify pin identity and continuity.
   - Internal pins remain safely parked in analog high-impedance mode (`GPIO_MODE_ANALOG`).

### How to Run `pin_check`

1. Set jumpers to **Development mode** (`JP5 -> 5V_STLK`, `JP1 -> Open`, `JP3 -> Closed`).
2. Connect the Nucleo board via its ST-LINK USB connector.
3. Build the diagnostic binary:
   ```bash
   cmake --build --preset Debug --target pin_check
   ```
4. Run the automated test with semihosting capture:
   ```bash
   cmake --build --preset Debug --target run_pin_check
   ```
5. Inspect the generated report:
   - Check the console output or open `pin_report.json`.
   - Open [docs/wiring_diagram.html](wiring_diagram.html) in your browser: it automatically loads `pin_report.json` and highlights any faulty pins, open circuits, or shorted nets directly on the physical board diagram.

---

## 5. External Circuit (Analog Front-End & Protection)

Once the bare interconnections are verified with `pin_check`, assemble the analog front-end (AFE) circuit. It provides input protection, impedance matching, and probe connectors. It can be constructed on a stripboard / protoboard (custom PCB files and stripboard layout will be provided in this section).

### Bill of Materials (BOM)
- **Schottky Diodes**: 6× Schottky diodes with $V_R \ge 40\text{ V}$ and low junction capacitance (BAS40-04 dual-diodes or equivalent discretes).
- **Resistors**:
  - 2× $1\text{ M}\Omega$ (channel input impedance / pull-down)
  - 3× $3\dots 6\text{ k}\Omega$ (series protection & current limiting)
- **Connectors**:
  - BNC or header pins for Channel A and Channel B probe inputs.
  - Dedicated connector/terminal for **Virtual Ground (VGND)**.
  - Pin headers mating with the Nucleo-64 Morpho headers (`CN7` / `CN10`).

### Schematic & Protection Overview
- **True Bipolar Range**: Measurable range is −1.65 V to +1.65 V around the internally generated **Virtual Ground** (1:10 attenuating probes recommended). Safe continuous input range: −30 V to +30 V.
- **Virtual Ground Notice**:
  > ⚠️ **Never connect oscilloscope probe ground clips to board GND!**
  > Always connect probe ground clips to the dedicated **Virtual Ground (VGND)** terminal. Connecting to board GND will short the internal mid-rail bias and distort measurements.

### Interactive Wiring Diagram
Refer to [docs/wiring_diagram.html](wiring_diagram.html) for exact pin and connector mappings on the ST Morpho headers (`CN7` and `CN10`).

*(Note: Stripe board / protoboard wiring diagram and layout drawings will be added here).*
