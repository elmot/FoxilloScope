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
SINE_PERIOD_SAMPLES = 83
SQUARE_PERIOD_SAMPLES = 200


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
        self.sample_cursor = 0
        self.trigger_armed = False
        self.trigger_holdoff_until = 0
        self.last_trigger_sample: int | None = None
        self.last_capture_start: int | None = None
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
            self.trigger_armed = False
            self.trigger_holdoff_until = self.sample_cursor

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
    def _signal_voltage(channel: str, sample_index: int) -> float:
        if channel == "a":
            phase = 2 * math.pi * sample_index / SINE_PERIOD_SAMPLES
            return 750_000 * math.sin(phase)
        phase = sample_index % SQUARE_PERIOD_SAMPLES
        return 450_000 if phase < SQUARE_PERIOD_SAMPLES / 2 else -450_000

    def _samples(self, channel: str, start_sample: int) -> list[int]:
        minimum_uv, maximum_uv = self._voltage_range(channel)
        return [
            self._to_adc(self._signal_voltage(channel, start_sample + index), minimum_uv, maximum_uv)
            for index in range(FRAME_SIZE)
        ]

    def _trigger_threshold_uv(self) -> float:
        channel = "a" if self.parameters["trg.chan"] == 0 else "b"
        gain = max(1.0, self.parameters[f"gain.{channel}"])
        center = self.parameters[f"base.lvl.{channel}.uv"]
        ppm = self.parameters["trigger.lvl.ppm"]
        return center + ppm / 1_000_000 * (BASE_VOLTAGE_UV / 2) / gain

    def _find_trigger(self, start_sample: int) -> int | None:
        trigger_type = self.parameters["trg.type"]
        if trigger_type == 0:
            self.trigger_armed = False
            return None

        channel = "a" if self.parameters["trg.chan"] == 0 else "b"
        threshold_uv = self._trigger_threshold_uv()
        minimum_uv, maximum_uv = self._voltage_range(channel)
        hysteresis_uv = (maximum_uv - minimum_uv) / ADC_STEPS * 2

        for sample_index in range(start_sample, start_sample + FRAME_SIZE):
            voltage_uv = self._signal_voltage(channel, sample_index)
            if trigger_type > 0:
                if not self.trigger_armed and voltage_uv <= threshold_uv - hysteresis_uv:
                    self.trigger_armed = True
                elif self.trigger_armed and voltage_uv >= threshold_uv + hysteresis_uv:
                    self.trigger_armed = False
                    if sample_index >= self.trigger_holdoff_until:
                        return sample_index
            else:
                if not self.trigger_armed and voltage_uv >= threshold_uv + hysteresis_uv:
                    self.trigger_armed = True
                elif self.trigger_armed and voltage_uv <= threshold_uv - hysteresis_uv:
                    self.trigger_armed = False
                    if sample_index >= self.trigger_holdoff_until:
                        return sample_index
        return None

    def build_frame(self) -> str:
        self.last_trigger_sample = None
        self.last_capture_start = None
        trigger_sample = self._find_trigger(self.sample_cursor)
        keyframe = trigger_sample is not None
        capture_start = self.sample_cursor
        if trigger_sample is not None:
            offset_samples = round(self.parameters["trg.time.offset"] * FRAME_SIZE / 1_000_000)
            capture_start = trigger_sample + offset_samples
            self.trigger_holdoff_until = trigger_sample + FRAME_SIZE
            self.last_trigger_sample = trigger_sample
            self.last_capture_start = capture_start
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
                    f"data.{channel}={encode_samples(self._samples(channel, capture_start))}",
                )
            )
        self.sample_cursor += FRAME_SIZE
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
