from __future__ import annotations

import argparse
import asyncio
import contextlib
import math
from pathlib import Path

try:
    from aiohttp import WSMsgType, web
except ModuleNotFoundError:
    WSMsgType = None
    web = None


BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
FRAME_SIZE = 400
ADC_STEPS = 4096
BASE_VOLTAGE_UV = 3_300_000
HTML_DIR = Path(__file__).resolve().parent.parent / "html"
SINE_FREQUENCY_HZ = 50
SQUARE_FREQUENCY_HZ = 20
SINE_AMPLITUDE_UV = 750_000
SQUARE_AMPLITUDE_UV = 450_000
NANOSECONDS_PER_SECOND = 1_000_000_000


def encode_samples(samples: list[int]) -> str:
    encoded = []
    for sample in samples:
        value = max(0, min(ADC_STEPS - 1, int(sample)))
        encoded.append(BASE64_CHARS[(value >> 6) & 0x3F])
        encoded.append(BASE64_CHARS[value & 0x3F])
    return "".join(encoded)


class MockOscilloscope:
    def __init__(self) -> None:
        self.parameters = {
            "sampling.ns": 500_000.0,
            "gain.a": 1.0,
            "gain.b": 1.0,
            "base.lvl.a.uv": 0.0,
            "base.lvl.b.uv": 0.0,
            "trg.type": 1.0,
            "trg.chan": 0.0,
            "trg.time.offset": 0.0,
            "trigger.lvl.ppm": 0.0,
        }
        self.time_cursor_ns = 0.0
        self.trigger_holdoff_until_ns = 0.0
        self.last_trigger_time_ns: float | None = None
        self.last_capture_start_ns: float | None = None
        self.version_requested = False

    def handle_commands(self, text: str) -> None:
        reset_trigger = False
        for line in text.splitlines():
            if "=" not in line:
                continue
            name, raw_value = (part.strip() for part in line.split("=", 1))
            if name == "version":
                self.version_requested = True
                continue
            if name not in self.parameters:
                continue
            try:
                self.parameters[name] = float(raw_value)
                if name.startswith(("trg.", "gain.", "base.lvl.")) or name == "trigger.lvl.ppm":
                    reset_trigger = True
            except ValueError:
                continue
        if reset_trigger:
            self.trigger_holdoff_until_ns = self.time_cursor_ns

    def _voltage_range(self, channel: str) -> tuple[float, float]:
        gain = max(1.0, self.parameters[f"gain.{channel}"])
        center = self.parameters[f"base.lvl.{channel}.uv"]
        span = BASE_VOLTAGE_UV / gain
        return center - span / 2, center + span / 2

    @staticmethod
    def _to_adc(voltage_uv: float, minimum_uv: float, maximum_uv: float) -> int:
        normalized = (voltage_uv - minimum_uv) / (maximum_uv - minimum_uv)
        return round(max(0.0, min(1.0, normalized)) * (ADC_STEPS - 1))

    @staticmethod
    def _signal_voltage(channel: str, time_ns: float) -> float:
        if channel == "a":
            phase = 2 * math.pi * SINE_FREQUENCY_HZ * time_ns / NANOSECONDS_PER_SECOND
            return SINE_AMPLITUDE_UV * math.sin(phase)
        phase = SQUARE_FREQUENCY_HZ * time_ns / NANOSECONDS_PER_SECOND % 1
        return SQUARE_AMPLITUDE_UV if phase < 0.5 else -SQUARE_AMPLITUDE_UV

    def _samples(self, channel: str, start_time_ns: float, sampling_ns: float) -> list[int]:
        minimum_uv, maximum_uv = self._voltage_range(channel)
        return [
            self._to_adc(
                self._signal_voltage(channel, start_time_ns + index * sampling_ns),
                minimum_uv,
                maximum_uv,
            )
            for index in range(FRAME_SIZE)
        ]

    def _trigger_threshold_uv(self) -> float:
        channel = "a" if self.parameters["trg.chan"] == 0 else "b"
        gain = max(1.0, self.parameters[f"gain.{channel}"])
        center = self.parameters[f"base.lvl.{channel}.uv"]
        ppm = self.parameters["trigger.lvl.ppm"]
        return center + ppm / 1_000_000 * (BASE_VOLTAGE_UV / 2) / gain

    def _next_trigger_time_ns(self, start_time_ns: float, sampling_ns: float) -> float | None:
        trigger_type = self.parameters["trg.type"]
        if trigger_type == 0:
            return None

        channel = "a" if self.parameters["trg.chan"] == 0 else "b"
        threshold_uv = self._trigger_threshold_uv()
        minimum_uv, maximum_uv = self._voltage_range(channel)
        hysteresis_uv = (maximum_uv - minimum_uv) / ADC_STEPS * 2
        amplitude_uv = SINE_AMPLITUDE_UV if channel == "a" else SQUARE_AMPLITUDE_UV
        if threshold_uv - hysteresis_uv < -amplitude_uv or threshold_uv + hysteresis_uv > amplitude_uv:
            return None

        frequency_hz = SINE_FREQUENCY_HZ if channel == "a" else SQUARE_FREQUENCY_HZ
        period_ns = NANOSECONDS_PER_SECOND / frequency_hz
        if channel == "a":
            target_uv = threshold_uv + hysteresis_uv if trigger_type > 0 else threshold_uv - hysteresis_uv
            phase = math.asin(target_uv / amplitude_uv)
            if trigger_type > 0:
                crossing_offset_ns = (phase % (2 * math.pi)) / (2 * math.pi) * period_ns
            else:
                crossing_offset_ns = (math.pi - phase) / (2 * math.pi) * period_ns
        else:
            crossing_offset_ns = 0 if trigger_type > 0 else period_ns / 2

        earliest_ns = max(start_time_ns, self.trigger_holdoff_until_ns) + sampling_ns
        cycle = math.ceil((earliest_ns - crossing_offset_ns) / period_ns)
        return cycle * period_ns + crossing_offset_ns

    def build_frame(self) -> str:
        sampling_ns = max(1.0, self.parameters["sampling.ns"])
        frame_duration_ns = FRAME_SIZE * sampling_ns
        self.last_trigger_time_ns = None
        self.last_capture_start_ns = None
        trigger_time_ns = self._next_trigger_time_ns(self.time_cursor_ns, sampling_ns)
        keyframe = trigger_time_ns is not None
        capture_start_ns = self.time_cursor_ns
        if trigger_time_ns is not None:
            offset_ns = self.parameters["trg.time.offset"] * frame_duration_ns / 1_000_000
            capture_start_ns = trigger_time_ns + offset_ns
            self.trigger_holdoff_until_ns = trigger_time_ns + frame_duration_ns
            self.last_trigger_time_ns = trigger_time_ns
            self.last_capture_start_ns = capture_start_ns
        lines = [
            f"frame.size={FRAME_SIZE}",
            "head=1",
            f"sampling.ns={self.parameters['sampling.ns']:g}",
            f"vltg.steps={ADC_STEPS}",
        ]
        if keyframe:
            lines.append("keyframe=1")
        if self.version_requested:
            lines.append("version=mock-server")
            self.version_requested = False
        for channel in ("a", "b"):
            minimum_uv, maximum_uv = self._voltage_range(channel)
            lines.extend(
                (
                    f"vltg.min.uv.{channel}={minimum_uv:g}",
                    f"vltg.max.uv.{channel}={maximum_uv:g}",
                    f"data.{channel}={encode_samples(self._samples(channel, capture_start_ns, sampling_ns))}",
                )
            )
        next_time_ns = self.time_cursor_ns + frame_duration_ns
        if trigger_time_ns is not None:
            next_time_ns = max(next_time_ns, trigger_time_ns + sampling_ns)
        self.time_cursor_ns = next_time_ns
        return "\n".join(lines)


async def stream_frames(ws: web.WebSocketResponse, device: MockOscilloscope, fps: float) -> None:
    while not ws.closed:
        await ws.send_str(device.build_frame())
        await asyncio.sleep(1 / fps)


def create_app(fps: float = 20.0) -> web.Application:
    if web is None:
        raise RuntimeError("aiohttp is required; install dependencies with: pip install -r requirements.txt")

    async def index_handler(request: web.Request) -> web.FileResponse:
        return web.FileResponse(HTML_DIR / "index.html")

    async def ws_handler(request: web.Request) -> web.WebSocketResponse:
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        device = MockOscilloscope()
        stream_task = asyncio.create_task(stream_frames(ws, device, fps))
        try:
            async for message in ws:
                if message.type == WSMsgType.TEXT:
                    device.handle_commands(message.data)
                elif message.type == WSMsgType.ERROR:
                    break
        finally:
            stream_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await stream_task
        return ws

    app = web.Application()
    app.router.add_get("/", index_handler)
    app.router.add_get("/ws", ws_handler)
    app.router.add_static("/", HTML_DIR)
    return app


def main() -> None:
    parser = argparse.ArgumentParser(description="Serve the FoxilloScope UI with synthetic waveforms.")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--fps", type=float, default=20.0)
    args = parser.parse_args()
    if args.fps <= 0:
        parser.error("--fps must be greater than zero")
    if web is None:
        parser.error("aiohttp is required; install dependencies with: pip install -r requirements.txt")
    web.run_app(create_app(args.fps), host=args.host, port=args.port)


if __name__ == "__main__":
    main()
