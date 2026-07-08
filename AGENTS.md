# Project: G4 Oscilloscope

## Architecture
- **Frontend**: Single `html/index.html` containing all HTML, CSS, and JS, watch `AGENTS.md` in the folder
- **Backend**: Python WebSocket server (`py/chart_server.py`) relays between browser and serial MCU — **agents do not need to modify this file**
- **Data path**: 
  - Debug: MCU → serial → Python → WebSocket → browser (base64 ADC → µV)
  - Production/wired: MCU → serial → WebSerial → browser
  - Production/wireless: MCU → serial → ESP32 web server → WebSocket → browser