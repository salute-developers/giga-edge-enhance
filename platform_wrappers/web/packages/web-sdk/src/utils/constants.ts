export const SPENCH_FRAME_SIZE_SECONDS = 0.016;

export const TRANSPORT_SIZE_SECONDS = SPENCH_FRAME_SIZE_SECONDS * 2;

export const HEAP_SIZE = 2048;

export const FEATURE_SWITCH_CACHE_MILLISECONDS = 60 * 1000;

export const DEFAULT_LP_WINDOW_SECONDS = 1;

export const DEFAULT_LP_MAX_REFILL_TIMES = 4;

export const DEFAULT_LP_REFILL_TIME_FACTOR = 3;

export const DEFAULT_FETCH_RETRIES = 2;

export const LP_CAUSE_THRESHOLD = 13;

export const MEAN_PROCESS_TIME_COUNT = 60;

export const MAX_SPIN_BLOCK_TIME_MS = 48;

export const INPUT_BUFFER_SECONDS = 0.064;

export const OUTPUT_BUFFER_SECONDS = 0.256;

export const MAX_WORKER_BLOCK_TIME_SECONDS = 3;

export const WORKLET_OUTPUT_BUFFER_MIN_LATENCY = 0.064;

export const WORKLET_OUTPUT_BUFFER_STEP = 0.032;

export const ASYNC_WAIT_TIMEOUT = 1000;

export const SYNC_WAIT_TIMEOUT = 1;

export const MAX_SEMAPHORE_WAIT_TIME_MS = 5000;

export const MAX_MESSAGE_WAIT_TIME_MS = 5;

// Max frame time measurements in worker before submit (near 5s)
export const TELEMETRY_FRAME_TIME_MAX_COUNT = 300;

export const SPENCH_WORKLET_NAME = 'spench-processor';

export const SPENCH_DSP_WORKLET_NAME = 'spench-dsp-processor';

export function getInputBufferSize(sampleRate: number) {
    return Math.ceil(sampleRate * INPUT_BUFFER_SECONDS);
}

export function getOutputBufferSize(sampleRate: number) {
    return Math.ceil(sampleRate * OUTPUT_BUFFER_SECONDS);
}

export const getDefaultNormalizationConfig = (sampleRate: number) => `
sound_processing:
  loudness_ctrl:
    sample_rate: ${sampleRate}
  dynamic_range_compressor_ctrl:
    sample_rate: ${sampleRate}
  noise_gate_ctrl:
    sample_rate: ${sampleRate}
  limiter_ctrl:
    sample_rate: ${sampleRate}
`;

export const getDefaultAECConfig = (sampleRate: number) => `
echo_cancellation:
  sample_rate: ${sampleRate}
`;
