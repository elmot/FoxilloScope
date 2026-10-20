import unittest

from mock_server import ADC_STEPS, FRAME_SIZE, MockOscilloscope, encode_samples


class EncodeSamplesTest(unittest.TestCase):
    def test_encodes_twelve_bit_boundaries(self) -> None:
        self.assertEqual(encode_samples([0, ADC_STEPS - 1]), "AA//")

    def test_clamps_values_to_twelve_bits(self) -> None:
        self.assertEqual(encode_samples([-1, ADC_STEPS]), "AA//")


class MockOscilloscopeTest(unittest.TestCase):
    def test_commands_update_supported_parameters(self) -> None:
        device = MockOscilloscope()
        device.handle_commands("sampling.ns=250\ngain.a=8\nunknown=12\n")

        self.assertEqual(device.parameters["sampling.ns"], 250)
        self.assertEqual(device.parameters["gain.a"], 8)
        self.assertNotIn("unknown", device.parameters)

    def test_frame_uses_frontend_protocol(self) -> None:
        device = MockOscilloscope()
        device.handle_commands("version=1")

        values = dict(line.split("=", 1) for line in device.build_frame().splitlines())

        self.assertEqual(values["frame.size"], str(FRAME_SIZE))
        self.assertEqual(values["head"], "1")
        self.assertEqual(values["keyframe"], "1")
        self.assertEqual(values["version"], "mock-server")
        self.assertEqual(len(values["data.a"]), FRAME_SIZE * 2)
        self.assertEqual(len(values["data.b"]), FRAME_SIZE * 2)

    def test_gain_changes_reported_input_range(self) -> None:
        device = MockOscilloscope()
        device.handle_commands("gain.a=10\nbase.lvl.a.uv=100000")

        minimum_uv, maximum_uv = device._voltage_range("a")

        self.assertEqual(minimum_uv, -65_000)
        self.assertEqual(maximum_uv, 265_000)

    def test_trigger_offset_positions_crossing_in_keyframe(self) -> None:
        device = MockOscilloscope()
        device.handle_commands("trg.time.offset=-250000")

        values = dict(line.split("=", 1) for line in device.build_frame().splitlines())

        self.assertEqual(values["keyframe"], "1")
        sampling_ns = device.parameters["sampling.ns"]
        trigger_index = (device.last_trigger_time_ns - device.last_capture_start_ns) / sampling_ns
        self.assertEqual(trigger_index, FRAME_SIZE // 4)

    def test_trigger_uses_selected_channel_and_slope(self) -> None:
        rising = MockOscilloscope()
        rising.handle_commands("trg.chan=1\ntrg.type=1")
        rising.build_frame()

        falling = MockOscilloscope()
        falling.handle_commands("trg.chan=1\ntrg.type=-1")
        falling.build_frame()

        self.assertEqual(rising.last_trigger_time_ns, 50_000_000)
        self.assertEqual(falling.last_trigger_time_ns, 25_000_000)

    def test_unreachable_threshold_does_not_create_keyframe(self) -> None:
        device = MockOscilloscope()
        device.handle_commands("trigger.lvl.ppm=1000000")

        values = dict(line.split("=", 1) for line in device.build_frame().splitlines())

        self.assertNotIn("keyframe", values)

    def test_trigger_type_zero_disables_keyframes(self) -> None:
        device = MockOscilloscope()
        device.handle_commands("trg.type=0")

        values = dict(line.split("=", 1) for line in device.build_frame().splitlines())

        self.assertNotIn("keyframe", values)

    def test_timescale_changes_visible_signal_span(self) -> None:
        device = MockOscilloscope()
        slow_samples = device._samples("a", 0, 500_000)
        fast_samples = device._samples("a", 0, 5_000)

        def midpoint_crossings(samples: list[int]) -> int:
            pairs = zip(samples, samples[1:])
            return sum((left < ADC_STEPS / 2) != (right < ADC_STEPS / 2) for left, right in pairs)

        self.assertGreater(midpoint_crossings(slow_samples), 10)
        self.assertEqual(midpoint_crossings(fast_samples), 0)


if __name__ == "__main__":
    unittest.main()
