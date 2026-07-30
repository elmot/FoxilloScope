# ESP32‑Gateway (esp32‑gateway) – Agent Documentation

## Purpose
The ESP32‑gateway firmware bridges the STM32 oscilloscope to the web client.
It hosts an HTTP server and a WebSocket endpoint that forwards ADC samples from the MCU to connected browsers.
Also, the gateway runs a BLE NUS-like protocol endpoint that mirrors UART traffic in the same way as the WebSocket.
Only one client at the time is allowed.


## Main Components
- The gateway runs both AP and STA mode. The oscilloscope is visible in both modes, but AP mode is primarily used for STA credentials setup.
- **Web Server** – Serves the HTML UI (`/`) and static resources.
- **WebSocket (`/ws`)** – Streams ADC data directly from the UART to the browser.
- **UART (Serial) Interface** – Reads raw ADC data from the MCU and forwards it transparently over the open WebSocket or BLE transport.
- **STM32 System Bootloader** is utilized for STM32G474 part firmware updates. Commmand 'bootloader=<magic_constant>' activates the stm32 bootloader. 
Refer to ST AN3155 appnote for more details.
- The gateway is present as `f-scope.local` host to the local network using mDNS and DNS(in case of AP connection).

## Build & Flash
```sh
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash
```

## Runtime Flow
1. ESP32 boots and connects to the configured Wi‑Fi network.
2. The HTTP server starts (default port 80, configurable via `CONFIG_HTTP_PORT`).
3. Browser loads `http://<esp32_ip>/` or `http://f-scope.local/` and receives the same `html/index.html`.
4. Browser opens a WebSocket to `ws:/ws` (or uses BLE transport if enabled).
5. The UART ISR receives MCU samples and forwards each byte unchanged over the WebSocket (or BLE) to the client.
6. When the connection disconnects, the server cleans up and waits for the next client.

## Configuration Files
- **`sdkconfig.secrets`** – Holds Wi‑Fi credentials and is __never committed to VCS__. The required keys are defined in `sdkconfig.secrets.template`.
- **`sdkconfig.secrets.template`** – Provides the variable names; the user should copy it to `sdkconfig.secrets` and fill in the values.

## Interaction with the Rest of the Project
- **Backend Python server** (`py/chart_server.py`) is used only for the *Debug* data path (serial → Python → WS) and is **not required** when the ESP32 gateway is used.

---
*This file is the definitive reference for agents interacting with the ESP32‑gateway portion of the G4 Oscilloscope project.*

## References
 - [Human-readable project description](../README.md) 
 - [Agents instructions for STM32 part and general project ideas](../AGENTS.md) 
 - [Web part description](../html/AGENTS.md)