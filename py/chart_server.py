import asyncio
import sys
import serial_asyncio
from serial import SerialException
import aiohttp
from aiohttp import web
from typing import Union

WS_CLIENTS: set[web.WebSocketResponse] = set()
serial_writer: Union[asyncio.StreamWriter, None] = None


async def serial_reader(reader: asyncio.StreamReader) -> None:
    buf: Union[list[str], None] = None
    try:
        while True:
            raw = await reader.readline()
            if not raw:
                await asyncio.sleep(0.001)
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if line == "[frame]":
                if buf is not None:
                    block_text = "\n".join(buf)
                    for ws in WS_CLIENTS.copy():
                        try:
                            await ws.send_str(block_text)
                        except ConnectionResetError:
                            WS_CLIENTS.discard(ws)
                buf = []
            elif buf is not None:
                buf.append(line)
    except SerialException as e:
        for ws in WS_CLIENTS.copy():
            try:
                await ws.send_str(f"error: {e}")
            except ConnectionResetError:
                WS_CLIENTS.discard(ws)


async def ws_handler(request: web.Request) -> web.WebSocketResponse:
    global serial_writer
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    WS_CLIENTS.add(ws)
    try:
        async for msg in ws:
            if msg.type == aiohttp.WSMsgType.TEXT:
                user_request = msg.data + "\n"
                serial_writer.write(user_request.encode("ascii"))
                await serial_writer.drain()
            elif msg.type == aiohttp.WSMsgType.ERROR:
                break
    finally:
        WS_CLIENTS.discard(ws)
    return ws


async def index_handler(request: web.Request) -> web.FileResponse:
    return web.FileResponse("../html/index.html")


def main():
    global serial_writer
    port = sys.argv[1] if len(sys.argv) > 1 else "COM11"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 460800

    async def on_startup(app):
        global serial_writer
        reader, writer = await serial_asyncio.open_serial_connection(
            url=port, baudrate=baud
        )
        serial_writer = writer
        app["serial_task"] = asyncio.create_task(serial_reader(reader))

    async def on_shutdown(app):
        app["serial_task"].cancel()
        if serial_writer is not None:
            serial_writer.close()
            await serial_writer.wait_closed()

    app = web.Application()
    app.router.add_get("/", index_handler)
    app.router.add_get("/ws", ws_handler)
    app.on_startup.append(on_startup)
    app.on_shutdown.append(on_shutdown)
    web.run_app(app, host="127.0.0.1", port=8000)


if __name__ == "__main__":
    main()
