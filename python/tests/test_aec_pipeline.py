from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest

from giga_edge_enhance import (
    AGC,
    Compressor,
    Denoiser,
    EchoCanceller,
    EchoCancellerConfig,
    Limiter,
    LoudnessControl,
    LoudnessControlConfig,
    NoiseGate,
    Observer,
    Pipeline,
    RangeControl,
    Resampler,
    VoiceActivityDetector,
)


def echo_signal(seconds=4, rate=16000):
    rng = np.random.default_rng(4127)
    reference = rng.normal(0, 0.1, int(seconds * rate)).astype(np.float32)
    echo = 0.65 * np.roll(reference, 96) + 0.2 * np.roll(reference, 180)
    echo[:180] = 0
    return echo.astype(np.float32), reference


def test_aec_echo_reduction_double_talk_and_absent_playback():
    microphone, reference = echo_signal()
    with EchoCanceller(config=EchoCancellerConfig(frame_size=256, filter_length=2048)) as aec:
        output = aec.process(microphone, reference)
        assert output.shape == microphone.shape
        assert np.mean(output[-16000:] ** 2) < np.mean(microphone[-16000:] ** 2) * 0.03
        near = np.zeros_like(microphone)
        near[-16000:] = 0.05 * np.sin(np.arange(16000) * 2 * np.pi * 313 / 16000)
        double_talk = aec.process(microphone + near, reference)
        assert np.sqrt(np.mean(double_talk[-8000:] ** 2)) > 0.015
        preserved = aec.process(near, np.zeros_like(reference))
        # Speex retains its native filtering even with silent playback. Check
        # speech energy here; the independent consumer below checks exact parity.
        ratio = np.mean(preserved[-8000:] ** 2) / np.mean(near[-8000:] ** 2)
        assert 0.7 < ratio < 1.3


@pytest.mark.parametrize(
    "residual,delay,premute",
    [(False, False, 0), (True, False, 0), (False, True, 0), (True, True, 0.4)],
)
def test_aec_options_chunking_reset_and_final_partial_frame(residual, delay, premute):
    microphone, reference = echo_signal(0.7)
    microphone, reference = microphone[:-7], reference[:-7]
    config = EchoCancellerConfig(
        frame_size=256,
        filter_length=1024,
        residual_suppression=residual,
        delay_compensation=delay,
        premute_seconds=premute,
    )
    with EchoCanceller(config=config) as aec:
        expected = aec.process(microphone, reference)
        parts = [
            aec.process_chunk(mic, ref)
            for mic, ref in zip(np.array_split(microphone, 37), np.array_split(reference, 37))
        ]
        parts.append(aec.flush())
        np.testing.assert_allclose(np.concatenate(parts), expected, atol=1 / 32767)
        assert aec.flush().size == 0
        with pytest.raises(RuntimeError, match="flushed"):
            aec.process_chunk(microphone, reference)
        aec.reset()
        reset = np.concatenate((aec.process_chunk(microphone, reference), aec.flush()))
        np.testing.assert_allclose(reset, expected, atol=1 / 32767)


def test_aec_invalid_pairs_and_pcm16_empty_input():
    with EchoCanceller() as aec:
        empty = np.empty(0, np.int16)
        assert aec.process(empty, empty).dtype == np.int16
        for mic, ref in (
            (np.zeros(4, np.float32), np.zeros(5, np.float32)),
            (np.zeros((4, 2), np.float32), np.zeros((4, 2), np.float32)),
            (np.zeros(4, np.int16), np.zeros(4, np.float32)),
            (np.full(4, 2, np.float32), np.zeros(4, np.float32)),
        ):
            with pytest.raises(ValueError):
                aec.process_chunk(mic, ref)
        audio = np.arange(517, dtype=np.int16)
        assert aec.process(audio, np.zeros_like(audio)).dtype == np.int16


def test_independent_aec_sessions_run_in_parallel():
    microphone, reference = echo_signal(0.5)
    with EchoCanceller() as model:
        expected = model.process(microphone, reference)
        with ThreadPoolExecutor(max_workers=2) as pool:
            jobs = [pool.submit(model.process, microphone, reference) for _ in range(2)]
            for job in jobs:
                np.testing.assert_allclose(job.result(), expected, atol=1 / 32767)


def component_reference(mode, config, audio, reference, tmp_path):
    root = os.environ.get("GEE_PYTHON_REFERENCE_ROOT")
    if root is None:
        pytest.skip("Run tools/test to compare an independent C++ SDK consumer")
    executable = (
        Path(root)
        / "dsp/python-reference"
        / ("enhance-python-components.exe" if os.name == "nt" else "enhance-python-components")
    )
    assert executable.is_file(), "Rebuild with tools/build to prepare C++ component consumer"
    configuration, source, playback, destination = [
        tmp_path / name for name in ("config.json", "input.f32", "reference.f32", "output.f32")
    ]
    configuration.write_text(json.dumps(config))
    audio.tofile(source)
    reference.tofile(playback)
    subprocess.run(
        [str(executable), mode, str(configuration), str(source), str(playback), str(destination)],
        check=True,
        timeout=60,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return np.fromfile(destination, np.float32)


@pytest.mark.parametrize(
    "factory", [AGC, RangeControl, LoudnessControl, NoiseGate, Limiter, Compressor]
)
def test_each_dsp_matches_public_cpp_sdk_consumer(factory, tmp_path):
    audio = np.random.default_rng(134).normal(0, 0.1, 4807).astype(np.float32)
    with factory() as processor:
        expected = component_reference(
            processor._kind,
            dict(sample_rate=48000, parameters=asdict(processor.config)),
            audio,
            np.empty(0, np.float32),
            tmp_path,
        )
        np.testing.assert_allclose(processor.process(audio), expected, atol=2e-6)


@pytest.mark.parametrize("silent_playback", [False, True])
def test_aec_matches_public_cpp_sdk_consumer(tmp_path, silent_playback):
    microphone, reference = echo_signal(0.6)
    microphone, reference = microphone[:-13], reference[:-13]
    if silent_playback:
        reference = np.zeros_like(reference)
    config = dict(
        echo_cancellation=dict(
            type="speex",
            sample_rate=16000,
            chunk_len_ms=16,
            filter_len=1024,
            residual_echo_suppression_enabled=False,
            delay_compensator_enabled=False,
            echo_premute_ms=400,
        )
    )
    expected = component_reference("aec", config, microphone, reference, tmp_path)
    with EchoCanceller(
        config=EchoCancellerConfig(frame_size=256, filter_length=1024, premute_seconds=0.4)
    ) as aec:
        np.testing.assert_allclose(aec.process(microphone, reference), expected, atol=1 / 32767)


def test_pipeline_uses_explicit_rates_and_reports_sample_positions():
    audio = np.random.default_rng(815).normal(0, 0.1, 8007).astype(np.float32)
    with Resampler(16000, 48000) as resampler, Limiter() as limiter:
        with Pipeline([resampler, limiter]) as pipeline:
            expected = limiter.process(resampler.process(audio))
            np.testing.assert_allclose(pipeline.process(audio), expected, atol=2e-6)
            pieces = []
            for chunk in np.array_split(audio, 13):
                start = pipeline.output_position
                timed = pipeline.process_timed_chunk(chunk)
                assert timed.start_sample == start and timed.sample_rate == 48000
                assert timed.time_seconds == start / 48000
                pieces.append(timed.audio)
            pieces.append(pipeline.flush())
            assert pipeline.input_position == len(audio)
            np.testing.assert_allclose(np.concatenate(pieces), expected, atol=2e-6)
        with pytest.raises(ValueError, match="sample rates"):
            Pipeline([limiter, resampler])


def test_aec_denoiser_dsp_pipeline_matches_manual_causal_composition(root, backend):
    microphone, reference = echo_signal(0.4)
    with Denoiser(root / "models/quality48" / backend, backend=backend) as model:
        with (
            EchoCanceller() as aec,
            model.stream(sample_rate=16000) as denoiser,
            Limiter(sample_rate=16000) as limiter,
        ):
            with Pipeline([aec, denoiser, limiter]) as pipeline:
                model.close()
                pairs = list(zip(np.array_split(microphone, 11), np.array_split(reference, 11)))
                actual = list(pipeline.process_chunks(iter(pairs)))
                manual = []
                for mic, ref in pairs:
                    manual.append(
                        limiter.process_chunk(denoiser.process_chunk(aec.process_chunk(mic, ref)))
                    )
                manual.append(limiter.process_chunk(denoiser.process_chunk(aec.flush())))
                manual.append(limiter.process_chunk(denoiser.flush()))
                manual.append(limiter.flush())
                np.testing.assert_allclose(
                    np.concatenate(actual), np.concatenate(manual), atol=2e-6
                )
                recording = pipeline.process(microphone, reference)
                assert recording.shape == microphone.shape
                causal = np.concatenate(actual)
                np.testing.assert_allclose(
                    recording,
                    causal[pipeline.latency_samples : pipeline.latency_samples + len(microphone)],
                    atol=2e-6,
                )
                with pytest.raises(ValueError, match="reference"):
                    pipeline.process(microphone)


def test_observers_preserve_audio_and_bound_results():
    audio = np.random.default_rng(128).normal(0, 0.1, 1607).astype(np.float32)
    with VoiceActivityDetector() as vad, Observer(vad) as observer:
        with Pipeline([observer]) as pipeline:
            np.testing.assert_array_equal(pipeline.process_chunk(audio), audio)
            assert sum(result.sample_count for result in pipeline.stages[0].observations) == 1600
            pipeline.flush()
            assert pipeline.stages[0].observations[0].sample_count == 7


@pytest.mark.parametrize(
    "use_vad,post", [(False, False), (False, True), (True, False), (True, True)]
)
def test_loudness_variants_expose_bypass_state_and_process_finite_audio(use_vad, post):
    with LoudnessControl(
        sample_rate=16000,
        config=LoudnessControlConfig(use_vad=use_vad, post_processing=post, bypass_time=0.02),
    ) as control:
        assert control.state[0].bypass_active
        output = control.process_chunk(np.zeros(640, np.float32))
        assert np.isfinite(output).all()
        assert not control.state[0].bypass_active
