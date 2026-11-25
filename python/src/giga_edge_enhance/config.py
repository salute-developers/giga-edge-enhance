"""DSP parameters use seconds, Hz, dB and linear power as named."""

from dataclasses import dataclass


@dataclass(frozen=True)
class AGCConfig:
    target_power_level: float = 1.0
    max_gain_db: float = 20.0
    average_length: int | None = None
    rise_step: float = 0.01
    fall_step: float = 0.01


@dataclass(frozen=True)
class RangeControlConfig:
    target_level_db: float = -3.0
    max_gain_db: float = 10.0
    attack_time: float = 1e-5
    release_time: float = 300.0
    hold_time: float = 5.0


@dataclass(frozen=True)
class LoudnessControlConfig:
    target_level_db: float = -23.0
    bypass_time: float = 3.0
    max_gain_db: float = 40.0
    use_vad: bool = False
    post_processing: bool = False


@dataclass(frozen=True)
class NoiseGateConfig:
    threshold_db: float = -15.0
    attack_time: float = 0.05
    release_time: float = 0.02
    hold_time: float = 0.05


@dataclass(frozen=True)
class LimiterConfig:
    threshold_db: float = -3.0
    knee_width_db: float = 0.0
    attack_time: float = 0.0
    release_time: float = 0.2


@dataclass(frozen=True)
class CompressorConfig:
    threshold_db: float = -15.0
    ratio: int = 5
    knee_width_db: float = 0.0
    attack_time: float = 0.01
    release_time: float = 0.2


@dataclass(frozen=True)
class LoudnessState:
    momentary_lufs: float | None = None
    short_term_lufs: float | None = None
    sample_peak_dbfs: float = -80.0


@dataclass(frozen=True)
class LoudnessControlState:
    gain_db: float = 0.0
    short_term_lufs: float | None = None
    maximum_lufs: float = 0.0
    minimum_lufs: float = -80.0
    bypass_active: bool = False


@dataclass(frozen=True)
class VadResult:
    """Decision for input samples [start_sample, start_sample + sample_count)."""

    start_sample: int
    sample_count: int
    score: float
    speech: bool


@dataclass(frozen=True)
class EchoCancellerConfig:
    frame_size: int = 512
    filter_length: int = 4096
    delay_compensation: bool = False
    residual_suppression: bool = False
    premute_seconds: float = 0.0
