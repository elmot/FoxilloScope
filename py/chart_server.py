import asyncio
import sys
import serial
import aiohttp
from aiohttp import web

WS_CLIENTS: set[web.WebSocketResponse] = set()

global serial_port

async def serial_reader():
    loop = asyncio.get_event_loop()
    buf: list[str] = []
    in_block = False
    try:
        while True:
            raw = await loop.run_in_executor(None, serial_port.readline)
            if not raw:
                await asyncio.sleep(0.001)
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if line == "[start]":
                in_block = True
                buf = []
            elif line == "[stop]" and in_block:
                block_text = "\n".join(buf)
                for ws in WS_CLIENTS.copy():
                    try:
                        await ws.send_str(block_text)
                    except ConnectionResetError:
                        WS_CLIENTS.discard(ws)
                in_block = False
            elif in_block:
                buf.append(line)
    except serial.SerialException as e:
        for ws in WS_CLIENTS.copy():
            try:
                await ws.send_str(f"error: {e}")
            except ConnectionResetError:
                WS_CLIENTS.discard(ws)
    finally:
        if serial_port.is_open:
            serial_port.close()


async def ws_handler(request: web.Request) -> web.WebSocketResponse:
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    WS_CLIENTS.add(ws)
    try:
        async for msg in ws:
            if msg.type == aiohttp.WSMsgType.TEXT:
                user_request = msg.data + "\n"
                serial_port.write(user_request.encode("ascii"))
            elif msg.type == aiohttp.WSMsgType.ERROR:
                break
    finally:
        WS_CLIENTS.discard(ws)
    return ws


async def index_handler(request: web.Request) -> web.FileResponse:
    return web.FileResponse("index.html")

def main():
    global serial_port
    port = sys.argv[1] if len(sys.argv) > 1 else "COM4"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 460800
    serial_port = serial.Serial(port, baud, timeout=0.01)
    app = web.Application()
    app.router.add_get("/", index_handler)
    app.router.add_get("/ws", ws_handler)

    async def on_startup(app):
        app["serial_task"] = asyncio.create_task(serial_reader())

    async def on_shutdown(app):
        app["serial_task"].cancel()

    app.on_startup.append(on_startup)
    app.on_shutdown.append(on_shutdown)

    web.run_app(app, host="127.0.0.1", port=8000)


if __name__ == "__main__":
    main()
