import asyncio
import base64
import sys
import serial
import aiohttp
from aiohttp import web

WS_CLIENTS: set[web.WebSocketResponse] = set()


def decode_12bit_b64(b64: str) -> list[int]:
    raw = base64.b64decode(b64)
    out = []
    for i in range(0, len(raw) - 2, 3):
        out.append((raw[i] << 4) | (raw[i + 1] >> 4))
        out.append(((raw[i + 1] & 0x0F) << 8) | raw[i + 2])
    return out


async def serial_reader(port: str, baud: int):
    loop = asyncio.get_event_loop()
    ser = serial.Serial(port, baud, timeout=0.01)
    buf: dict[str, str] = {}
    in_block = False
    try:
        while True:
            raw = await loop.run_in_executor(None, ser.readline)
            if not raw:
                await asyncio.sleep(0.001)
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if line == "[start]":
                in_block = True
                buf.clear()
            elif in_block:
                if "=" not in line:
                    continue
                k, v = line.split("=", 1)
                k = k.strip().replace(".", "_")
                v = v.strip()
                buf[k] = v
                if k == "data_a":
                    samples = decode_12bit_b64(v)
                    payload = {
                        "sampling_freq": int(buf.get("sampling_freq", 0)),
                        "shift_a": int(buf.get("shift_a", 0)),
                        "gain_a": int(buf.get("gain_a", 1)),
                        "data_a": samples,
                    }
                    for ws in WS_CLIENTS.copy():
                        try:
                            await ws.send_json(payload)
                        except ConnectionResetError:
                            WS_CLIENTS.discard(ws)
                    in_block = False
                    buf.clear()
    except serial.SerialException as e:
        for ws in WS_CLIENTS.copy():
            try:
                await ws.send_json({"error": str(e)})
            except ConnectionResetError:
                WS_CLIENTS.discard(ws)
    finally:
        if ser.is_open:
            ser.close()


async def ws_handler(request: web.Request) -> web.WebSocketResponse:
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    WS_CLIENTS.add(ws)
    try:
        async for msg in ws:
            if msg.type == aiohttp.WSMsgType.ERROR:
                break
    finally:
        WS_CLIENTS.discard(ws)
    return ws


async def index_handler(request: web.Request) -> web.FileResponse:
    return web.FileResponse("index.html")


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM4"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    app = web.Application()
    app.router.add_get("/", index_handler)
    app.router.add_get("/ws", ws_handler)

    async def on_startup(app):
        app["serial_task"] = asyncio.create_task(serial_reader(port, baud))

    async def on_shutdown(app):
        app["serial_task"].cancel()

    app.on_startup.append(on_startup)
    app.on_shutdown.append(on_shutdown)

    web.run_app(app, host="127.0.0.1", port=8000)


if __name__ == "__main__":
    main()
