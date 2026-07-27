# Frontend Architecture & Technical Reference (`html/`)

This directory contains the single-page web frontend for the G4 Oscilloscope.

---

## 1. Multi-Transport Subsystem (`utils.js`)

The application abstracts physical transport layers behind the unified `comm` object (`comm.switchTo(mode)`). 

### Supported Transports
- **`wifi`**: WebSocket connection to `ws://${location.host}/ws`. Directly receives text frame buffers.
- **`serial`**: WebSerial API (`navigator.serial`) running at **460800 baud**. Consumes continuous binary byte streams via `createFrameReader`.
- **`ble`**: WebBluetooth GATT (`6623a8e1-77d3-4e35-a01c-4d649ff5fb07`).
  - **TX Characteristic** (`...a8e2`): Receives notifications fed into `createFrameReader`.
  - **RX Characteristic** (`...a8e3`): Uses an internal FIFO queue (`_writeQueue`) with non-blocking async `_flush()` calling `writeValueWithoutResponse()`.

### Frame Stream Parsing & Custom Base64 Decoding
- **Stream Framing**: Stream chunks are aggregated into a line buffer and delimited by `#` (`createFrameReader`).
- **Base64 Sample Decoding (`decode(s)`)**:
  - ADC samples are binary-packed into 6-bit Base64 character pairs (2 chars per 12-bit sample).
  - Lookup dictionary `_B64` decodes character pairs:
    $$\text{Sample} = (\text{Base64}[c_0] \ll 6) \mid \text{Base64}[c_1]$$

---

## 2. Parameter System & Hardware Controls (`scope.js`)

Parameters are persisted in `localStorage` under `vslParameters` and managed via `Hardware`.

### Logarithmic HW / SW Gain Splitting (`Gain` object)
- Slider values represent logarithmic gain ($\text{min} = 0$, $\text{max} = \ln(504) \approx 6.22$).
- Total Gain: $G_{\text{total}} = \text{clamp}(e^{\text{slider}}, 1, 504)$.
- Display Range: $\text{range.uv} = \frac{V_{\text{base}}}{G_{\text{total}}}$, where $V_{\text{base}} = 3,300,000\,\mu\text{V}$.
- **HW/SW Gain Split (`Gain.splitGain(total)`)**:
  - Iterates hardware PGA steps: $\text{HW\_GAINS} = [64, 32, 16, 8, 4, 2, 1]$.
  - Finds the largest $G_{\text{hw}} \le G_{\text{total}}$ such that software multiplier $G_{\text{sw}} = \frac{G_{\text{total}}}{G_{\text{hw}}} \le 8$.
  - Hardware command sent to MCU: `gain.<channel>=<G_hw>`.

### Trigger Level & Vertical Offset Math
- **Trigger Level (`trigger.lvl.ppm`)**: Stored as Parts Per Million (PPM) of the visible screen height ($-500,000$ to $+500,000$, i.e., $\pm 50\%$ span).
- **PPM to $\mu\text{V}$ Mapping (`Hardware.triggerLevelUv()`)**:
  $$V_{\text{trg}}(\mu\text{V}) = V_{\text{base.lvl}}(\mu\text{V}) + V_{\text{range}}(\mu\text{V}) \times \frac{\text{trigger.lvl.ppm}}{1,000,000}$$
  *(uses $V_{\text{range}}$ and $V_{\text{base.lvl}}$ of the active trigger channel `trg.chan`)*.
- **Hardware Command**: `trg.level=<ppm>` sent via `sendTriggerParameters()`.

---

## 3. Data Frame Protocol & Waveform Reconstruction

Incoming frame strings parsed in `onFrame(text)` deliver parameters and packed waveform buffers.

### Frame Control Commands
- `param?`: Sent by MCU when re-initialized; frontend responds by executing `Hardware.sendAllParameters()`.
- `keyframe=1`: Indicates keyframe storage. Rendered as a persistent ghost trace with opacity decaying over time:
  $$\alpha = \max\left(30, 250 - \left\lceil 50 \times \frac{T_{\text{now}} - T_{\text{key}}}{T_{\text{frame\_duration}}} \right\rceil\right)$$
- `head=1`: Signals the first segment of a new frame, resetting the sample buffer array. Subsequent chunks append until complete.

### ADC Sample to Microvolt Conversion (`updatePlot()`)
Each raw sample $S[i] \in [0, \text{steps}]$ (typically 4096 steps) is mapped to voltage using MCU-calibrated minimum/maximum limits (`vltg.min.uv.<ch>`, `vltg.max.uv.<ch>`):
$$V[i] = V_{\text{min}} + S[i] \times \frac{V_{\text{max}} - V_{\text{min}}}{\text{steps}}$$
The resulting voltage is clamped to the channel's active display window $[V_{\text{base}} - \frac{V_{\text{range}}}{2}, V_{\text{base}} + \frac{V_{\text{range}}}{2}]$.

---

## 4. uPlot Custom Hooks & Gesture Interactions

The chart uses [uPlot](https://github.com/leeoniya/uPlot) with 5 data series (Time, ChA Live, ChB Live, ChA Keyframe, ChB Keyframe).

### Custom Canvas Drawing (`drawAxes` hook)
- **Zero-Volt Baseline**: Computes $y = \text{valToPos}(0, \text{ch})$ for ChA and ChB; draws horizontal dashed lines in respective channel colors.
- **Trigger Level Line**: Renders horizontal pink dashed line at $y = \text{valToPos}(V_{\text{trg}}, \text{ch})$.
- **Time Shift Marker**: Renders vertical pink dashed line at the zero-time trigger offset point clipped within axis bounds.

### Axis Direct Touch & Pointer Capture
- **X-Axis Drag (`axes[0]`)**: Pointer drag adjusts horizontal trigger offset `vslParameters["trg.time.offset"]` (PPM).
- **Y-Axes Drag (`axes[1]` for ChA, `axes[2]` for ChB)**: Pointer vertical drag modifies channel DC bias `vslParameters.channels[ch]["base.lvl.uv"]`.
- **Measurement Cursor (`setCursor` hook)**: Displays live readout ($\Delta t$, $\Delta V_{\text{A}}$, $\Delta V_{\text{B}}$) when dragging a selection rectangle on the viewport.

---

## 5. Development & Modification Rules

1. **DO NOT TOUCH `uPlot.iife.min.js` or `uPlot.min.css`**.
2. **Preserve Single-Page Architecture**: Keep UI minimal, fast, and dependency-free (vanilla JS).
3. **PPM vs $\mu\text{V}$ Discipline**:
   - UI screen-relative controls (trigger position, horizontal offset) MUST use PPM.
   - Hardware channel offsets and absolute voltages MUST use $\mu\text{V}$.
4. **Logarithmic Scaling**: Always maintain log-space mapping for gain sliders to ensure consistent feel across large dynamic ranges.
