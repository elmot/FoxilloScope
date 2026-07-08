# Project: G4 Oscilloscope

## Architecture
- **Frontend**: Single `index.html` containing all HTML, CSS, and JS
- **Charting**: uPlot with two y-axes (Ch A left, Ch B right), common time x-axis
- **Data path**: MCU → serial → WebSerial or WebSockets → browser (base64 ADC → µV)

## Key Files
- `index.html` — the entire UI (HTML structure, CSS styling, all JS logic)

## Communication
- Controls sent as `key=value\n` via WebSocket (sendAllCommands iterates `param._controls`)
- Server responds with data frames (base64 samples + voltage range info)

## UX Intentions
- **Professional oscilloscope feel** — clean, no clutter; controls look like a real bench scope
- **Single continuous sliders** — gain and bias each use ONE slider; the HW/SW split is an invisible implementation detail
- **Logarithmic gain** — slider maps to log space so equal thumb travel = equal ratio change (×1 to ×504)
- **No bias readout** — bias value is an implementation detail; the user sees the trace shift, not a number
- **Channel coloring** — Ch A gold (#FFD600) left, Ch B cyan (#00E5FF) right; TRG green (#0f0) with Ch A since it's global
- **Vertical side-by-side sliders** — gain/bias/trg sliders are vertical (`writing-mode: sideways-lr`)
- **Full-screen fit** — page fills the viewport with zero scrolling; use flex/grid sizing, never `overflow:hidden`
- **Session persistence** — slider positions survive page refresh via cookies; old-format cookies are migrated transparently
- **Param system hygiene** — tracking data (gain log positions, bias totals) MUST NOT live in `param._values`; use `_track` object and `scope_track` cookie to prevent accidental WebSocket sends
- **Zero-jump bias** — `Math.trunc` for HW split avoids the discontinuity that `Math.round` causes at ±0.5 boundaries
- **Readability** — gain readout shows `×N.NN` in channel color, HW+SW breakdown in subdued gray (#686868)

## Frontend Architecture

### Params System (`param` object in JS)
- `param.register(id, element, updateFn)` — registers a control; adds to `_controls` map
- `param.set(id, val)` — updates local value + sends `id=val\n` over WebSocket
- `param._values` — raw value store (persisted in cookie `scope_params`)
- `param._controls` — registered element map; used by `sendAllCommands()` to re-send on reconnect
- **Only registered params are sent via WebSocket**

### Channel Colors (CSS custom properties)
- `--ch-a`: `#FFD600` (gold)
- `--ch-b`: `#00E5FF` (cyan)
- TRG: green `#0f0`

### Gain Control (Custom — NOT param-slider)
- **Class**: `gain-slider` (NOT `param-slider` — bypasses generic handler)
- Single continuous slider per channel
- Slider stores **log position** (0–1000), not gain value directly
- `posToGain(pos)` = `exp(pos/1000 * ln(504))`, rounded to 0.01
- `gainToPos(gain)` = inverse
- `splitGain(totalGain)` → {hw, sw}: picks largest HW gain (63,31,15,6,3,1) ≤ total with SW zoom ≤ 8
- HW gain sent to device (`gain.a` / `gain.b`)
- SW zoom stored in `swState[ch].zoom`, applied in `updateDisplayRange()`
- Tracking key: `gain.{ch}.pos` in `_track.gainPos`
- Readout: `×N.NN` (gain, channel-colored), `HW+SW` (detail, gray)

### Bias Control (Custom — NOT param-slider)
- **Class**: `bias-slider`
- Continuous slider ±1,000,000, step 0.1
- `Math.trunc(total)` → HW bias sent to device (`vbias.a` / `vbias.b`)
- Residual fraction → SW offset in µV: `residual * range / 2000000`
- No numeric readout shown
- Tracking key: `vbias.{ch}.total` in `_track.biasTotal`

### Trigger Control (Generic param-slider)
- `trg.level` uses `param-slider`, registered by generic handler
- `trg.chan` / `trg.type` use `button-switch-block`, registered by generic button handler
- TRG slider is in left panel with Ch A, but colored green (not gold)

### Display Range
- `rawUvRange` = constant: {minA:-1650000, maxA:1650000, minB:-1650000, maxB:1650000}
- `updateDisplayRange()` computes effective range: center ± halfRange/zoom, shifted by SW offset
- Result stored in `vltg.minUv{A,B}` / `vltg.maxUv{A,B}`
- Called in slider `apply()` + `uplot.redraw()`

### Page Layout (Desktop / Landscape)
```
body {flex-direction: column}
  #scope-wrap {flex:1; display:flex}
    .v-controls-a (240px, gold)  →  #scope (flex:1)  →  .v-controls-b (160px, cyan)
  #bottom-controls (sampling + trg shift)
```
- Sliders use `writing-mode: sideways-lr` for vertical orientation
- Full-screen fit via flex sizing — never use `overflow:hidden`

### Page Layout (Portrait mobile, ≤900px)
```
#scope-wrap {display:grid; grid-template-columns:1fr 1fr; grid-template-rows:1fr auto}
  #scope        — row 1, full width (scope chart)
  .v-controls-a — row 2, col 1 (Ch A controls, vertical sliders, 130px tall)
  .v-controls-b — row 2, col 2 (Ch B controls, vertical sliders, 130px tall)
```
- Controls sit below the chart, side by side
- Sliders remain vertical (`sideways-lr`)
- `#footer` hidden

### Page Layout (Landscape mobile, ≤500px height)
```
.v-controls-a — flex: 0 0 95px  (was 240px)
.v-controls-b — flex: 0 0 70px  (was 160px)
```
- Side panels narrowed to give chart more horizontal space
- Compact fonts, `#footer` hidden

### Cookie Persistence
- Params cookie: `scope_params` (JSON encoded from `param._values` — only registered controls)
- Tracking cookie: `scope_track` (JSON encoded from `_track` object — gain positions, bias totals)
- Tracking data (`gainPos`, `biasTotal`) stored in `_track` object, never in `param._values`
- On load: `param._loadCookie()` restores registered params; IIFE reads `scope_track` (via `_trackLoadCookie()`) to restore slider positions; falls back to old `gain.{ch}.total`/`vbias.{ch}.total` keys from `scope_params` for migration

## Important Conventions
- Never store gain/bias tracking keys (`gainPos`, `biasTotal`) in `param._values` — use `_track` object instead (avoids cookie pollution and prevents accidental WebSocket sends)
- Always call `_trackSaveCookie()` after modifying `_track`
- Always call `uplot.redraw()` after changing display range
- Use `Math.trunc` for bias HW split (not Math.round) to avoid zero-crossing discontinuities
- `sendAllCommands()` iterates `param._controls`, not `param._values`
- **`chart_server.py` is a simple WebSocket bridge — agents do not need to modify it**
- **Never use `overflow:hidden`** for layout — use flex/grid sizing instead
- **CSS must be mobile-friendly** — include `@media (orientation: portrait)` and `@media (orientation: landscape)` queries
