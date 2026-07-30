# G4 Oscilloscope – Root & STM32 Architecture Reference

4 MHz (8 MHz not implemented yet) sampling, dual-channel, 12-bit digital wireless oscilloscope powered by STM32G474 MCU and ESP32 module.
STM32G474 controls the on-chip analog front-end (AFE), FreeRTOS, and transfers data over UART. ESP32 receives the data and handles multi-transport streaming via Wi-Fi or BLE.
ESP32-C3 is used for now; other chips like ESP32, ESP32-C (3,5,6), ESP32-S (2,3), and others may be used, but are untested. 

---

## 1. System Architecture & Subsystem References

The project consists of three core subsystems:
- **STM32G474 Firmware** (this directory & `Core/`) – High-speed ADC sampling, hardware AFE control, trigger engine, and dual DMA UART streaming.
- **ESP32 Gateway Subsystem** – See [esp32-gateway/AGENTS.md](esp32-gateway/AGENTS.md) for WiFi/AP gateway, WebSocket forwarding (`/ws`), and in-application STM32 firmware flashing.
- **Web Frontend Subsystem** – See [html/AGENTS.md](html/AGENTS.md) for UI rendering, parameter scaling, and uPlot integration.

### Dual-Use Web Frontend Context
The single-page web UI (`html/index.html`) is designed for **dual deployment**:
1. **Local ESP32 Gateway**: Served directly by the ESP32 over HTTP (`http://f-scope.local/`), communicating via WebSocket (`ws://${location.host}/ws`).
2. **Static CDN / GitHub Pages**: Hosted externally over HTTPS, communicating directly with hardware via WebBluetooth GATT (`6623a8e1-...`) or WebSerial (460,800 baud USB COM port).

---

## 2. STM32G474 Architecture & Non-Obvious Implementation Details

### Analog Front-End (AFE) & Hardware PGA Bias
- **PGA Gain**: OPAMPs operate in internal PGA mode (gain $G \in [1..64]$).
- **DC Offset Math**: In PGA mode ($G > 1$), DAC drives the inverting bias node. Because bias scales with PGA feedback, DAC voltage calculation compensates non-linearly:
  $$V_{\text{dac}} = V_{\text{DD}}/2 + \text{bias} \times \frac{G}{G - 1}$$
- **Virtual Ground**: DAC generates virtual ground, buffered by OPAMP as a follower output.

### ADC Acquisition ($\le 4\text{ MHz}$)
- **ADC Allocation**: ADCs sample full buffers triggered by sampling timer TRGO.
- **DMA Driver**: Master ADC DMA acts as interrupt driver (`HAL_ADC_ConvHalfCpltCallback` / `HAL_ADC_ConvCpltCallback`); `adcSamplesLeft()` monitors remaining transfer counts ([adc.c](Core/Src/adc.c#L602-L649)).

### Hardware Comparator & Single-Pulse Trigger Engine
- **Comparator Setup**: Hardware comparators compare AFE outputs against DAC threshold levels.
- **Timer Trigger Chain**: Comparator edge interrupt (`HAL_COMP_TriggerCallback`) arms a single-pulse PWM timer that gates sampling timer clocking. Upon pulse completion (`HAL_TIM_PWM_PulseFinishedCallback`), timers stop, raising `THREAD_FLAG_KEY_FRAME_DETECTED` to capture pre/post trigger window ([oscilloscope.cpp](Core/Src/oscilloscope.cpp#L433-L458)).

### Parallel Dual-UART DMA Transmission
- **Hardware Channels**: LPUART (USB Virtual COM) and UART (ESP32 Gateway interface) run in parallel via DMA (`writeUart`).
- **Synchronized Blocking**: Transmission of frame chunks blocks until DMA TC interrupt flags clear on **both** peripherals, guaranteeing simultaneous USB and ESP32 gateway throughput without data skew ([transmit.cpp](Core/Src/transmit.cpp#L100-L142)).

### In-Application Firmware Upgrade (ST Bootloader Jump)
Command `bootloader=45063` (`0xB007` magic constant) triggers `startSysBootloader()` ([main.c](Core/Src/main.c#L66-L90)),
that de-initializes HAL and IRQs, and then jumps to ST ROM bootloader.

### VREFINT Auto-Calibration
- ADC1 injected rank 1 measures internal $V_{\text{refint}}$ (`ADC_CHANNEL_VREFINT`).

---

## 3. Hardware Wiring & Peripherals Reference

For complete signal routing, pinout diagrams, and peripheral configurations, refer directly to `/docs`:
- **Peripheral & Pin Mapping**: [docs/pinout-peripherals.md](docs/pinout-peripherals.md) – Pin table for OPAMPs, DACs, ADCs, COMPs, TIMers, and UARTs.
- **Physical Wiring Diagram**: [docs/wiring.png](docs/wiring.png)
- **Board Pinout Map**: [docs/nucleo-g474re-pinout.png](docs/nucleo-g474re-pinout.png)
