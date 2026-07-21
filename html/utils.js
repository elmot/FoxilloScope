    function cssVar(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}
    const COLORS = {
    chA: cssVar('--ch-a'),
    chB: cssVar('--ch-b'),
    chAKey: cssVar('--ch-a-key'),
    chBKey: cssVar('--ch-b-key'),
};

    function createFrameReader(onFrame) {
        let buffer = '';
        return (chunk) => {
            buffer += chunk;
            const parts = buffer.split('#');
            buffer = parts.pop();
            for (const f of parts) if (f.trim()) onFrame(f.trim());
        };
    }
    const _B64 = Object.fromEntries("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".split("").map((c,i)=>[c,i]));
    function decode(s) { const r=[]; for(let i=0;i<s.length;i+=2){const a=_B64[s[i]],b=_B64[s[i+1]]; if(a!==undefined&&b!==undefined) r.push((a<<6)|b); } return r; }
    const transports = {
        wifi: {
            async connect(onFrame, onDisconnect) {
                const ws = new WebSocket(`ws://${location.host}/ws`); this.ws = ws;
                await new Promise((resolve, reject) => {
                    ws.onopen = () => resolve();
                    ws.onerror = () => reject(new Error("WebSocket connection failed"));
                    ws.onclose = () => { if (!this._intentionalClose && ws === this.ws) onDisconnect(); };
                    ws.onmessage = (e) => { if (typeof e.data === 'string') onFrame(e.data); };
                });
            },
            disconnect() { if (this.ws) { this.ws.close(); this.ws = null; } },
            send(data) { if (this.ws?.readyState === WebSocket.OPEN) this.ws.send(data); },
            statusText: "Connected",
        },
        serial: {
            async connect(onFrame, onDisconnect) {
                const port = await navigator.serial.requestPort(); await port.open({ baudRate: 460800 });
                this.serialPort = port;
                this._serialStop = false;
                const feed = createFrameReader(onFrame), dec = new TextDecoder();
                this._readLoop(port, feed, dec, onDisconnect);
            },
            async _readLoop(port, feed, dec, onDisconnect) {
                try {
                    while (port.readable && !this._serialStop) {
                        const r = port.readable.getReader();
                        this._serialReader = r;
                        try {
                            while (true) {
                                const { value, done } = await r.read();
                                if (done) break;
                                feed(dec.decode(value, { stream: true }));
                            }
                        } finally {
                            r.releaseLock();
                            if (this._serialReader === r) this._serialReader = null;
                        }
                    }
                }
                catch (e) { if (!this._intentionalClose && port === this.serialPort) onDisconnect(); }
            },
            disconnect() {
                if (this.serialPort) {
                    this._serialStop = true;
                    const port = this.serialPort;
                    const p = this._serialReader ? this._serialReader.cancel() : Promise.resolve();
                    p.then(() => port.close().catch(() => {})).catch(() => {});
                }
                this.serialPort = null;
                this._serialReader = null;
            },
            send(data) { const w = this.serialPort?.writable?.getWriter(); if (w) { w.write(new TextEncoder().encode(data)); w.releaseLock(); } },
            statusText: "serial connected", errorLabel: "serial error",
        },
        ble: {
            _svc: '6623a8e1-77d3-4e35-a01c-4d649ff5fb07',
            _tx: '6623a8e2-77d3-4e35-a01c-4d649ff5fb07',
            _rx: '6623a8e3-77d3-4e35-a01c-4d649ff5fb07',
            _writeQueue: [], _writing: false,
            async _flush() {
                if (this._writing) return;
                this._writing = true;
                while (this._writeQueue.length) {
                    const data = this._writeQueue.shift();
                    try { if (this.bleRxChar) await this.bleRxChar.writeValueWithoutResponse(data); }
                    catch (e) { console.error("BLE write failed:", e); }
                }
                this._writing = false;
            },
            async connect(onFrame, onDisconnect) {
                const device = await navigator.bluetooth.requestDevice({ filters: [{ services: [this._svc] }] });
                this.bleDevice = device;
                device.addEventListener('gattserverdisconnected', () => { if (!this._intentionalClose && device === this.bleDevice) onDisconnect(); });
                const svc = await (await device.gatt.connect()).getPrimaryService(this._svc);
                const txChar = await svc.getCharacteristic(this._tx);
                this.bleRxChar = await svc.getCharacteristic(this._rx);
                const feed = createFrameReader(onFrame);
                await txChar.startNotifications();
                txChar.addEventListener('characteristicvaluechanged', (ev) => feed(new TextDecoder().decode(ev.target.value)));
            },
            disconnect() { this._writeQueue = []; this._writing = false; if (this.bleDevice) { try { this.bleDevice.gatt.disconnect(); } catch(e) {} this.bleDevice = null; } this.bleRxChar = null; },
            send(data) { if (!this.bleRxChar) return; this._writeQueue.push(new TextEncoder().encode(data)); this._flush(); },
            statusText: "BLE connected", errorLabel: "BLE error",
        },
    };
