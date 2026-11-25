import subprocess
import sys

import numpy as np
import pytest

from giga_edge_enhance import (
    AGC,
    AudioProcessor,
    AGCConfig,
    Compressor,
    CompressorConfig,
    Limiter,
    LimiterConfig,
    LoudnessControl,
    LoudnessControlConfig,
    LoudnessMeter,
    MuteFrameDetector,
    NoiseGate,
    NoiseGateConfig,
    RangeControl,
    RangeControlConfig,
    Resampler,
    RNNoise,
    VoiceActivityDetector,
)


def signal(size=4807, rate=48000):
    return (
        0.4 * np.sin(np.arange(size, dtype=np.float32) * np.float32(2 * np.pi * 997 / rate))
    ).astype(np.float32)


@pytest.mark.parametrize(
    "factory", [AGC, RangeControl, LoudnessControl, NoiseGate, Limiter, Compressor, RNNoise]
)
def test_recording_chunking_reset_channels_and_lifetime(factory):
    audio = signal()
    with factory() as processor:
        expected = processor.process(audio)
        parts = [processor.process_chunk(part) for part in np.array_split(audio, 29)]
        parts.append(processor.flush())
        actual = np.concatenate(parts)[
            processor.latency_samples : processor.latency_samples + len(audio)
        ]
        np.testing.assert_allclose(actual, expected, atol=2e-6)
        assert expected.shape == audio.shape and np.isfinite(expected).all()
        assert processor.flush().size == 0
        with pytest.raises(RuntimeError, match="flushed"):
            processor.process_chunk(audio)
        processor.reset()
        reset = np.concatenate((processor.process_chunk(audio), processor.flush()))
        np.testing.assert_allclose(
            reset[processor.latency_samples : processor.latency_samples + len(audio)],
            expected,
            atol=2e-6,
        )
    processor.close()
    with pytest.raises(RuntimeError, match="closed"):
        processor.process_chunk(audio)
    with factory(channels=2) as stereo:
        output = stereo.process(np.column_stack((audio, -audio)))
        np.testing.assert_allclose(output[:, 0], expected, atol=2e-6)
        with factory() as mono:
            np.testing.assert_allclose(output[:, 1], mono.process(-audio), atol=2e-6)


@pytest.mark.parametrize("factory", [AGC, RangeControl, NoiseGate, Limiter, Compressor])
def test_native_gain_is_applied_to_output(factory):
    audio = signal()
    with factory() as processor:
        output = processor.process_chunk(audio)
        gain = processor.gain
        assert gain.shape == audio.shape
        np.testing.assert_allclose(output, audio * gain, atol=1e-7)
        saved = output.copy()
        processor.process_chunk(audio)
        np.testing.assert_array_equal(output, saved)


def test_ordered_chain_is_equivalent_to_stages_and_does_not_borrow_state():
    audio = signal()
    with AGC() as first, Limiter(config=LimiterConfig(threshold_db=-12)) as second:
        expected = second.process(first.process(audio))
        with AudioProcessor([first, second]) as chain:
            first.close()
            second.close()
            np.testing.assert_allclose(chain.process(audio), expected, atol=2e-6)
            streamed = np.concatenate(list(chain.process_chunks(np.array_split(audio, 11))))
            np.testing.assert_allclose(streamed, expected, atol=2e-6)
    with pytest.raises(ValueError, match="same sample_rate"):
        AudioProcessor([Limiter(sample_rate=16000), AGC()])


@pytest.mark.parametrize("rate", [8000, 16000, 24000, 32000, 44100])
def test_rnnoise_native_rate_conversion_has_complete_recording_and_score(rate):
    audio = signal(rate // 3 + 17, rate)
    with RNNoise(sample_rate=rate) as processor:
        result = processor.process(audio)
        assert result.shape == audio.shape and np.isfinite(result).all()
        pieces = [processor.process_chunk(chunk) for chunk in np.array_split(audio, 9)]
        pieces.append(processor.flush())
        actual = np.concatenate(pieces)[
            processor.latency_samples : processor.latency_samples + len(audio)
        ]
        np.testing.assert_allclose(actual, result, atol=2e-6)
        assert 0 <= processor.state[0]["score"] <= 1


def test_known_dsp_transfer_functions_and_bypass():
    audio = signal(1000)
    with Limiter(config=LimiterConfig(threshold_db=-12, attack_time=0, release_time=0)) as limiter:
        np.testing.assert_allclose(
            limiter.process(audio), np.clip(audio, -(10 ** (-12 / 20)), 10 ** (-12 / 20)), atol=2e-6
        )
    with Compressor(config=CompressorConfig(ratio=1)) as compressor:
        np.testing.assert_allclose(compressor.process(audio), audio, atol=1e-7)
    with RangeControl(config=RangeControlConfig(max_gain_db=0)) as control:
        np.testing.assert_array_equal(control.process(audio), audio)
    with LoudnessControl(config=LoudnessControlConfig(bypass_time=10)) as control:
        np.testing.assert_array_equal(control.process(audio), audio)
    with NoiseGate(
        config=NoiseGateConfig(threshold_db=-10, attack_time=0, release_time=0, hold_time=0)
    ) as gate:
        quiet = np.full(1000, 0.01, np.float32)
        np.testing.assert_array_equal(gate.process(quiet), np.zeros_like(quiet))
    with AGC(config=AGCConfig(max_gain_db=6)) as agc:
        agc.process_chunk(audio)
        assert np.max(agc.gain) <= 10 ** (6 / 20) + 1e-5


@pytest.mark.parametrize(
    "source,target", [(48000, 8000), (8000, 48000), (44100, 48000), (48000, 44100), (16000, 16000)]
)
def test_resampler_tiny_chunks_full_tail_and_stereo(source, target):
    audio = signal(3001, source)
    with Resampler(source, target) as resampler:
        expected = resampler.process(audio)
        parts = [resampler.process_chunk(audio[i : i + 1]) for i in range(len(audio))]
        parts.append(resampler.flush())
        actual = np.concatenate(parts)
        assert len(actual) == (len(audio) * target + source - 1) // source
        np.testing.assert_allclose(actual, expected, atol=2e-5)
        assert resampler.flush().size == 0
    with Resampler(source, target, channels=2) as stereo:
        result = stereo.process(np.column_stack((audio, -audio)))
        np.testing.assert_allclose(result[:, 0], expected, atol=2e-5)
        np.testing.assert_allclose(result[:, 1], -expected, atol=2e-5)


def test_resampler_ratio_is_explicit_and_resettable():
    with Resampler(44100, 48000) as resampler:
        resampler.set_io_ratio(44100 / 48000)
        assert np.isfinite(resampler.process_chunk(signal())).all()
        resampler.flush()
        with pytest.raises(RuntimeError, match="flushed"):
            resampler.set_io_ratio(1)
        resampler.reset()
        assert resampler.state[0]["delay_ms"] >= 0
    with pytest.raises(ValueError):
        Resampler(0, 48000)


def test_variable_resampling_ratio_changes_stream_duration():
    audio = signal(44100, 44100)
    with Resampler(44100, 48000) as resampler:
        resampler.set_io_ratio(0.9)
        result = np.concatenate((resampler.process_chunk(audio), resampler.flush()))
        assert abs(len(result) - len(audio) / 0.9) <= 3


def test_loudness_meter_availability_peak_and_empty_input():
    with LoudnessMeter() as meter:
        assert meter.state.momentary_lufs is None and meter.state.short_term_lufs is None
        meter.update(signal(240))
        assert meter.state.short_term_lufs is None
        meter.update(signal(48000))
        assert -16 < meter.state.momentary_lufs < -9
        assert meter.state.sample_peak_dbfs == pytest.approx(20 * np.log10(0.4), abs=0.01)
        before = meter.state
        meter.update(np.empty(0, np.float32))
        assert meter.state == before
        meter.reset()
        assert meter.state.short_term_lufs is None


@pytest.mark.parametrize("rate", [8000, 16000, 44100, 48000])
def test_vad_ranges_threshold_and_partial_frame(rate):
    audio = signal(rate // 3 + 7, rate)
    with VoiceActivityDetector(sample_rate=rate, threshold=1) as vad:
        expected = vad.analyze(audio)
        actual = []
        for part in np.array_split(audio, 17):
            actual.extend(vad.process_chunk(part))
        actual.extend(vad.flush())
        assert expected == actual
        assert sum(frame.sample_count for frame in actual) == len(audio)
        assert all(0 <= frame.score <= 1 and not frame.speech for frame in actual)
        assert [frame.start_sample for frame in actual] == list(
            range(0, len(audio), vad.frame_size)
        )
        assert vad.flush() == []
        vad.reset()
        assert vad.analyze(audio) == expected
        assert list(vad.process_chunks(iter(np.array_split(audio, 13)))) == expected


def test_gap_detector_reports_anomaly_without_treating_silence_as_gap():
    with MuteFrameDetector(160, sample_rate=16000) as detector:
        audio = np.full(160 * 8, 0.2, np.float32)
        audio[160 * 3 : 160 * 4] = 0
        flags = detector.process(audio)
        assert flags.dtype == np.bool_ and flags.shape == audio.shape
        assert np.any(flags[160 * 3 : 160 * 6])
        assert not detector.process(np.zeros_like(audio)).any()


@pytest.mark.parametrize(
    "factory",
    [
        lambda: Limiter(config=LimiterConfig(threshold_db=1)),
        lambda: Compressor(config=CompressorConfig(ratio=0)),
        lambda: NoiseGate(config=NoiseGateConfig(attack_time=-1)),
        lambda: LoudnessControl(config=LoudnessControlConfig(use_vad="yes")),
        lambda: AGC(config=AGCConfig(target_power_level=float("nan"))),
        lambda: VoiceActivityDetector(threshold=2),
        lambda: MuteFrameDetector(0),
    ],
)
def test_invalid_parameters_fail_before_native_processing(factory):
    with pytest.raises((TypeError, ValueError)):
        factory()


def test_dsp_import_does_not_load_inference_engines(tmp_path):
    subprocess.run(
        [
            sys.executable,
            "-c",
            "import sys; from giga_edge_enhance import Limiter; p=Limiter(); assert 'giga_edge_enhance._onnx' not in sys.modules; assert 'giga_edge_enhance._tflite' not in sys.modules; p.close()",
        ],
        cwd=tmp_path,
        check=True,
        timeout=60,
    )
