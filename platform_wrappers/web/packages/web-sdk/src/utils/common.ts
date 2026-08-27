import {
    DEFAULT_FETCH_RETRIES,
    DEFAULT_LP_MAX_REFILL_TIMES,
    DEFAULT_LP_REFILL_TIME_FACTOR,
    DEFAULT_LP_WINDOW_SECONDS,
    WORKLET_OUTPUT_BUFFER_MIN_LATENCY,
    WORKLET_OUTPUT_BUFFER_STEP,
} from './constants';

export type ModelKey = 'lite8k' | 'lite24k' | 'full24k' | 'lite48k' | 'full48k';

/**
 * A value that can represent a model. It can be a string URL, a `URL` instance,
 * or the raw `ArrayBuffer` containing the model data.
 */
export type ModelValue = string | URL | ArrayBuffer;

/**
 * A map from a main feature to the available model URLs for each {@link ModelKey}. The type is generic over the
 * model value type `V` to allow for flexibility.
 *
 * @template V - The type of the model value.
 */
export type ModelsTmpl<V = ModelValue> = Partial<Record<keyof MainFeatures<any>, Partial<Record<ModelKey, V>>>>;

export type MainFeatures<M extends ModelsTmpl> = {
    /** Enable/disable Noise Cancellation.  When truthy, the string must be a
     * {@link ModelKey} selecting the model to load. */
    noiseCancellation: boolean | keyof M['noiseCancellation'];
};

/**
 * Digital Signal Processing (DSP) features that can be toggled on the audio
 * thread.  These features do not require external model files.
 */
export type DspFeatures = {
    /** Enable/disable audio normalization. */
    normalization: boolean;
    /** Enable/disable acoustic echo cancellation. */
    echoCancellation: boolean;
};

/**
 * A full feature set that combines the main features and DSP features.
 *
 * @template M - The {@link ModelsTmpl} instance defining available models.
 */
export type Features<M extends ModelsTmpl> = MainFeatures<M> & DspFeatures;

/**
 * Configuration options for the Web SDK.
 *
 * @property lowPerformanceWindowSeconds   The time window (in seconds) used to count
 *                                          refill attempts before deciding that the node
 *                                          should emit a low‑performance event.
 * @property lowPerformanceMaxRefillTimes  The maximum number of buffer refills allowed
 *                                          before the low‑performance event is emitted.
 * @property lowPerformanceMaxRefillTimeFactor
 *                                      A output buffer size x this factor = max refill time
 *                                      to wait by the audio thread before stopping.
 * @property minLatency                    The initial buffer latency (in seconds) before any
 *                                          expansion occurs.
 * @property latencyIncreaseStep           The amount (in seconds) by which the latency is
 *                                          increased on each step.
 * @property maxProcessingChunkSeconds     The maximum processing chunk size (in seconds).
 * @property fetchRetries                  The maximum number of retry attempts for
 *                                          fetching resources before giving up.
 * @property sharedMemoryEnabled           Whether to use `SharedArrayBuffer` when the
 *                                          environment supports it.
 * @property forceSharedMemory             Force the SDK to use shared memory regardless
 *                                          of environment support.
 * @property dspOnAudioThread              (Deprecated) Whether to run DSP features on
 *                                          the audio thread.
 * @property useVadNormalization           Use Voice Activity Detection feature for
 *                                          the normalization feature.
 * @property unstable_reverseProcessingOrder
 *                                      (Experimental) Reverse processing order of ML
 *                                          and DSP features.
 * @property unstable_customNormalizationConfig
 *                                      (Experimental) Custom configuration string for
 *                                      the normalization feature.
 * @property unstable_customAECConfig      (Experimental) Custom configuration string for
 *                                          the echo‑cancellation feature.
 */
export type Settings = {
    lowPerformanceWindowSeconds: number;
    lowPerformanceMaxRefillTimes: number;
    lowPerformanceMaxRefillTimeFactor: number;
    minLatency: number;
    latencyIncreaseStep: number;
    maxProcessingChunkSeconds: number;
    fetchRetries: number;
    sharedMemoryEnabled: boolean;
    forceSharedMemory: boolean;
    dspOnAudioThread: boolean;
    useVadNormalization: boolean;
    unstable_reverseProcessingOrder: boolean;
    unstable_customNormalizationConfig?: string;
    unstable_customAECConfig?: string;
};

export function getSettingsWithDefaults(settings: Partial<Settings> = {}): Settings {
    return {
        ...settings,
        dspOnAudioThread: Boolean(settings.dspOnAudioThread),
        useVadNormalization: Boolean(settings.useVadNormalization),
        unstable_reverseProcessingOrder: Boolean(settings.unstable_reverseProcessingOrder),
        fetchRetries: typeof settings.fetchRetries === 'number' ? settings.fetchRetries : DEFAULT_FETCH_RETRIES,
        lowPerformanceMaxRefillTimeFactor:
            typeof settings.lowPerformanceMaxRefillTimeFactor === 'number'
                ? settings.lowPerformanceMaxRefillTimeFactor
                : DEFAULT_LP_REFILL_TIME_FACTOR,
        forceSharedMemory: Boolean(settings.forceSharedMemory),
        sharedMemoryEnabled:
            Boolean(settings.forceSharedMemory) ||
            (typeof settings.sharedMemoryEnabled === 'boolean' ? settings.sharedMemoryEnabled : true),
        lowPerformanceWindowSeconds:
            typeof settings.lowPerformanceWindowSeconds === 'number'
                ? settings.lowPerformanceWindowSeconds
                : DEFAULT_LP_WINDOW_SECONDS,
        lowPerformanceMaxRefillTimes:
            typeof settings.lowPerformanceMaxRefillTimes === 'number'
                ? settings.lowPerformanceMaxRefillTimes
                : DEFAULT_LP_MAX_REFILL_TIMES,
        minLatency: typeof settings.minLatency === 'number' ? settings.minLatency : WORKLET_OUTPUT_BUFFER_MIN_LATENCY,
        latencyIncreaseStep:
            typeof settings.latencyIncreaseStep === 'number'
                ? settings.latencyIncreaseStep
                : WORKLET_OUTPUT_BUFFER_STEP,
        maxProcessingChunkSeconds:
            typeof settings.maxProcessingChunkSeconds === 'number' ? settings.maxProcessingChunkSeconds : 0.016,
    };
}

export function areMainFeaturesEnabled<M extends ModelsTmpl>(features: Partial<MainFeatures<M>>) {
    return Object.values(getMainFeatures(features)).some(Boolean);
}

export function areDspFeaturesEnabled(features: Partial<DspFeatures>) {
    return Object.values(getDspFeatures(features)).some(Boolean);
}

export function areMainFeaturesChanged<M extends ModelsTmpl>(
    features: Partial<MainFeatures<M>>,
    newFeatures: Partial<MainFeatures<M>>,
) {
    const mainFeatures = getMainFeatures(features);
    const newMainFeatures = getMainFeatures(newFeatures);

    return (Object.keys(mainFeatures) as (keyof MainFeatures<M>)[]).some((feature) => {
        return mainFeatures[feature] !== newMainFeatures[feature];
    });
}

export function areDspFeaturesChanged(features: Partial<DspFeatures>, newFeatures: Partial<DspFeatures>) {
    const dspFeatures = getDspFeatures(features);
    const newDspFeatures = getDspFeatures(newFeatures);

    return (Object.keys(dspFeatures) as (keyof DspFeatures)[]).some((feature) => {
        return dspFeatures[feature] !== newDspFeatures[feature];
    });
}

export function isSharedMemorySupported() {
    return typeof SharedArrayBuffer === 'function' && typeof window !== 'undefined' && window.crossOriginIsolated;
}

export function getMainFeatures<M extends ModelsTmpl>(features: Partial<MainFeatures<M>> = {}): MainFeatures<M> {
    const { noiseCancellation = false } = features;

    return { noiseCancellation };
}

export function getDspFeatures(features: Partial<DspFeatures> = {}): DspFeatures {
    const { normalization = false, echoCancellation = false } = features;

    return { normalization, echoCancellation };
}

const modelTypeLoadOrder = ['lite24k', 'full24k', 'lite48k', 'full48k'] as const;
export function getDefaultNoiseCancellationModel(models: ModelsTmpl | undefined, sampleRate: number): ModelKey {
    if (sampleRate <= 8000 && models?.noiseCancellation?.lite8k) {
        return 'lite8k';
    }

    for (const type of modelTypeLoadOrder) {
        if (models?.noiseCancellation?.[type]) {
            return type;
        }
    }

    return 'full48k';
}

export async function downloadFileAsBuffer(
    url: string | URL,
    preload: boolean = false,
): Promise<[ArrayBuffer, number]> {
    const start = performance.now();

    const response = await fetch(url, preload ? { priority: 'low' } : undefined);

    if (!response.ok) {
        throw new Error(`Invalid response code: ${response.status} ${response.statusText}`);
    }

    const arrayBuffer = await response.arrayBuffer();

    const elapsed = Math.round(performance.now() - start);

    return [arrayBuffer, elapsed];
}

export async function retry<E extends Error, R>(
    fn: () => Promise<E | R>,
    retries = DEFAULT_FETCH_RETRIES,
): Promise<E | R> {
    let res: E | R;
    let i = 0;

    do {
        res = await fn();

        if (!(res instanceof Error)) {
            return res;
        }
    } while (i++ < retries);

    return res;
}

export function copyArrayBuffer(buffer: ArrayBuffer): ArrayBuffer {
    const res = new ArrayBuffer(buffer.byteLength);
    new Uint8Array(res).set(new Uint8Array(buffer));

    return res;
}
