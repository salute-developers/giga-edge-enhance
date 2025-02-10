export const HEAP_SIZE = 2048;

export const FEATURE_SWITCH_CACHE_MILLISECONDS = 60 * 1000;

export const OVERFLOW_COUNTER_WINDOW_SECONDS = 0.5;

export const WARMUP_STEP_COUNT = 50;

export const WORKLET_OUTPUT_BUFFER_SECONDS = 0.5;

export const WORKLET_OUTPUT_BUFFER_THRESHOLD = WORKLET_OUTPUT_BUFFER_SECONDS / 2;

// Max frame time measurements in worker before submit (near 5s)
export const TELEMETRY_FRAME_TIME_MAX_COUNT = 300;

export const SPENCH_WORKLET_NAME = 'spench-processor';

// TODO serialize model and load from the SDK
export const WASM_PATH = {
    denoiser: {
        full24k: 'data/config/mobile_group8_hybrid_loss_fp32.yaml',
        lite24k: 'data/config/fast_model_int8.yaml',
    },
    dereverb: {
        full48k: 'data/config/dereverb_vae_48khz_fp32.yaml',
    },
} as const;
