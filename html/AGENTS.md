# Frontend Architecture & Technical Reference (`html/`)

Single‑page web frontend for the G4 Oscilloscope.

---

## 1. Multi-Transport Subsystem (`scope.js`)

The `comm` object abstracts transports.

### Supported Transports
- `wifi`: WebSocket `ws://${location.host}/ws`.
- `serial`: WebSerial @ 460800 baud, binary stream via `createFrameReader`.
- `ble`: WebBluetooth GATT (`6623a8e1-...`).
  - TX (`...a8e2`): notifications to `createFrameReader`.
  - RX (`...a8e3`): FIFO `_writeQueue`, async `_flush()`.

### Frame Stream Parsing & Custom Base64 Decoding
- **Stream Framing**: Stream chunks are aggregated into a line buffer and delimited by `#` (`createFrameReader`). The delimiter character is stripped; payloads must not contain a raw `#` because it would be interpreted as a frame boundary.
- **Base64 Sample Decoding (`decode(s)`)**:
  - ADC samples are binary-packed into 6-bit Base64 character pairs (2 chars per 12-bit sample).
  - Lookup dictionary `_B64` decodes character pairs:
    $$\text{Sample} = (\text{Base64}[c_0] \ll 6) \mid \text{Base64}[c_1]$$

---

## 2. Parameter System & Hardware Controls (`scope.js`)

Parameters stored in `localStorage` as `vslParameters`.

### Logarithmic HW/SW Gain (`Gain`)
- Slider: log gain (min 0, max ln 504≈6.22).
- Total gain clamped to 1‑504.
- Range: `range.uv = V_base / G_total` (V_base = 3.3 MV).
- `Gain.splitGain` selects hardware PGA step from [64,32,16,8,4,2,1] and software multiplier ≤ 8.
- MCU command: `gain.<channel>=<G_hw>`.
- Voltage zoom software multiplier capped at 8 for 12‑bit ADC fidelity.

### Trigger Level & Vertical Offset Math
- Trigger level stored as PPM (‑500k to +500k).
- `Hardware.triggerLevelUv()`: `V_trg = V_base.lvl + V_range * trigger.lvl.ppm / 1e6`.
- Command: `trg.level=<ppm>`.
- `trg.type`: 0 = none, 1 = rising, –1 = falling.

---

## 3. Data Frame Protocol & Reconstruction
- `onFrame(text)` parses incoming frames.
### Control Commands
- `param?`: MCU init request → `Hardware.sendAllParameters()`.
- `version=1`: Request FW version → MCU replies with textual `version=YYYYMMDD-#####` (date and short git commit hash).
- `keyframe=1`: Store keyframe; ghost trace fades.
- `head=1`: Start new frame, reset buffer, then append chunks.

### ADC Sample to Microvolt Conversion (`updatePlot()`)
- Decoded 12-bit ADC sample integers are mapped to microvolts ($\mu\text{V}$) according to active channel gain and DC baseline offset (`base.lvl.uv`) before plotting on uPlot canvas.

---

## 4. uPlot Custom Hooks & Gesture Interactions

Chart uses [uPlot](https://github.com/leeoniya/uPlot) version **v1.6.x** 

### Custom Canvas Drawing (`drawAxes` hook)
- **Zero-Volt Baseline**: Computes $y = \text{valToPos}(0, \text{ch})$ for ChA and ChB; draws horizontal dashed lines in respective channel colors.
- **Trigger Level Line**: Renders horizontal pink dashed line at $y = \text{valToPos}(V_{\text{trg}}, \text{ch})$.
- **Time Shift Marker**: Renders vertical pink dashed line at the zero-time trigger offset point clipped within axis bounds.

### Axis Direct Touch & Pointer Capture
- **X-Axis Drag (`axes[0]`)**: Pointer drag adjusts horizontal trigger offset `vslParameters["trg.time.offset"]` (PPM).
- **Y-Axes Drag (`axes[1]` for ChA, `axes[2]` for ChB)**: Pointer vertical drag modifies channel DC bias `vslParameters.channels[ch]["base.lvl.uv"]`.
- Axes mapping: `axes[0]` = time (X), `axes[1]` = Y for Channel A, `axes[2]` = Y for Channel B.
- **Measurement Cursor (`setCursor` hook)**: Displays live readout ($\Delta t$, $\Delta V_{\text{A}}$, $\Delta V_{\text{B}}$) when dragging a selection rectangle on the viewport.

---

## 5. Development & Modification Rules

1. **Do not modify** `uPlot.iife.min.js` / `uPlot.min.css`
2. **Single‑page**: keep UI minimal, fast, vanilla JS.
3. **PPM vs μV**: UI offsets use PPM; hardware offsets use μV.
4. **Logarithmic scaling**: maintain log‑space gain sliders.
## 6. Deployment & Transport Security

- Serve locally via the ESP32 gateway (`../esp32-gateway/AGENTS.md`) over HTTP.
- GitHub Pages uses HTTPS; BLE and WebSerial work only in secure contexts.
- Provide HTTPS on the gateway if BLE/Serial are required in production.

## Resources
- [scope.js](file:///d:/projects/foxilloscope/html/scope.js) – Transport abstraction, frame parsing, parameter handling, UI logic, and hardware commands.

## Documentation Sources
- The UI relies on **uPlot** for charting; its official documentation and example gallery are the primary knowledge source for any future UI changes.

## Parameter Naming Conventions
- Suffix **`.uv`** denotes values in **microvolts**.
- Suffix **`.ns`** denotes values in **nanoseconds**.
- Suffix **`ppm`** denotes **parts‑per‑million**.
- All parameters are **numeric**; exception: `version` command (`version=1` sent to FW requests version reply; response is textual `version=YYYYMMDD-#####` with date and short git hash).

## Communication Session Init
- Initial exchange after transport establishment: command strings negotiate capabilities and request parameters.

## References
- [Human-readable project description](../README.md)
- [Agents instructions for STM32 part and general project ideas](../AGENTS.md)
- [ESP32 serial <-> WebSocket/BLE gateway](../esp32-gateway/AGENTS.md)