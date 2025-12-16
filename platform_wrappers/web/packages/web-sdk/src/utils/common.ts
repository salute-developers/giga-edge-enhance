import {
    DEFAULT_FETCH_RETRIES,
    DEFAULT_LP_MAX_REFILL_TIMES,
    DEFAULT_LP_REFILL_TIME_FACTOR,
    DEFAULT_LP_WINDOW_SECONDS,
    WORKLET_OUTPUT_BUFFER_MIN_LATENCY,
    WORKLET_OUTPUT_BUFFER_STEP,
} from './constants';

export type ModelKey = 'lite24k' | 'full24k' | 'lite48k' | 'full48k';

export type ModelValue = string | URL | ArrayBuffer;

export type ModelsTmpl<V = ModelValue> = Partial<Record<keyof MainFeatures<any>, Partial<Record<ModelKey, V>>>>;

export type MainFeatures<M extends ModelsTmpl> = {
    noiseCancellation: boolean | keyof M['noiseCancellation'];
    dereverberation: boolean | keyof M['dereverberation'];
};

export type DspFeatures = {
    normalization: boolean;
    echoCancellation: boolean;
};

export type Features<M extends ModelsTmpl> = MainFeatures<M> & DspFeatures;

export type Settings = {
    lowPerformanceWindowSeconds: number;
    lowPerformanceMaxRefillTimes: number;
    lowPerformanceMaxRefillTimeFactor: number;
    minLatency: number;
    latencyIncreaseStep: number;
    fetchRetries: number;
    sharedMemoryEnabled: boolean;
    dspOnAudioThread: boolean;
    unstable_customNormalizationConfig?: string;
    unstable_customAECConfig?: string;
};

export function getSettingsWithDefaults(settings: Partial<Settings> = {}): Settings {
    return {
        ...settings,
        dspOnAudioThread: Boolean(settings.dspOnAudioThread),
        fetchRetries: typeof settings.fetchRetries === 'number' ? settings.fetchRetries : DEFAULT_FETCH_RETRIES,
        lowPerformanceMaxRefillTimeFactor:
            typeof settings.lowPerformanceMaxRefillTimeFactor === 'number'
                ? settings.lowPerformanceMaxRefillTimeFactor
                : DEFAULT_LP_REFILL_TIME_FACTOR,
        sharedMemoryEnabled: typeof settings.sharedMemoryEnabled === 'boolean' ? settings.sharedMemoryEnabled : true,
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
    const { noiseCancellation = false, dereverberation = false } = features;

    return { noiseCancellation, dereverberation };
}

export function getDspFeatures(features: Partial<DspFeatures> = {}): DspFeatures {
    const { normalization = false, echoCancellation = false } = features;

    return { normalization, echoCancellation };
}

const modelTypeLoadOrder = ['lite24k', 'full24k', 'lite24k', 'full48k'] as const;
export function getDefaultNoiseCancellationModel(models: ModelsTmpl | undefined): ModelKey {
    for (const type of modelTypeLoadOrder) {
        if (models?.noiseCancellation?.[type]) {
            return type;
        }
    }

    return 'lite24k';
}

export function getDefaultDereverberationModel(models: ModelsTmpl | undefined): ModelKey {
    for (const type of modelTypeLoadOrder) {
        if (models?.dereverberation?.[type]) {
            return type;
        }
    }

    return 'lite24k';
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
