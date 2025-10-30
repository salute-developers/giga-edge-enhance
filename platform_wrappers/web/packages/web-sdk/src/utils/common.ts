import { DEFAULT_FETCH_RETRIES, DEFAULT_LP_MAX_REFILL_TIMES, DEFAULT_LP_WINDOW_SECONDS } from './constants';

type ModelOrigin = string;

export type Models = {
    noiseCancellation: { full24k: ModelOrigin; lite24k: ModelOrigin };
    dereverberation: { full48k: ModelOrigin };
};

export type MainFeatures = {
    noiseCancellation: boolean | keyof Models['noiseCancellation'];
    dereverberation: boolean | keyof Models['dereverberation'];
};

export type DspFeatures = {
    normalization: boolean;
    echoCancellation: boolean;
};

export type Features = MainFeatures & DspFeatures;

export type Settings = {
    lowPerformanceWindowSeconds: number;
    lowPerformanceMaxRefillTimes: number;
    dspOnAudioThread: boolean;
    fetchRetries: number;
    unstable_customNormalizationConfig?: string;
    unstable_customAECConfig?: string;
};

export function getSettingsWithDefaults(settings: Partial<Settings> = {}): Settings {
    return {
        ...settings,
        dspOnAudioThread: Boolean(settings.dspOnAudioThread),
        fetchRetries: typeof settings.fetchRetries === 'number' ? settings.fetchRetries : DEFAULT_FETCH_RETRIES,
        lowPerformanceWindowSeconds:
            typeof settings.lowPerformanceWindowSeconds === 'number'
                ? settings.lowPerformanceWindowSeconds
                : DEFAULT_LP_WINDOW_SECONDS,
        lowPerformanceMaxRefillTimes:
            typeof settings.lowPerformanceMaxRefillTimes === 'number'
                ? settings.lowPerformanceMaxRefillTimes
                : DEFAULT_LP_MAX_REFILL_TIMES,
    };
}

export function areMainFeaturesEnabled(features: Partial<Features>) {
    return Object.values(getMainFeatures(features)).some(Boolean);
}

export function areDspFeaturesEnabled(features: Partial<Features>) {
    return Object.values(getDspFeatures(features)).some(Boolean);
}

export function areMainFeaturesChanged(features: Partial<Features>, newFeatures: Partial<Features>) {
    const mainFeatures = getMainFeatures(features);
    const newMainFeatures = getMainFeatures(newFeatures);

    return (Object.keys(mainFeatures) as (keyof MainFeatures)[]).some((feature) => {
        return mainFeatures[feature] !== newMainFeatures[feature];
    });
}

export function areDspFeaturesChanged(features: Partial<Features>, newFeatures: Partial<Features>) {
    const dspFeatures = getDspFeatures(features);
    const newDspFeatures = getDspFeatures(newFeatures);

    return (Object.keys(dspFeatures) as (keyof DspFeatures)[]).some((feature) => {
        return dspFeatures[feature] !== newDspFeatures[feature];
    });
}

export function getMainFeatures(features: Partial<Features> = {}): MainFeatures {
    const { noiseCancellation = false, dereverberation = false } = features;

    return { noiseCancellation, dereverberation };
}

export function getDspFeatures(features: Partial<Features> = {}): DspFeatures {
    const { normalization = false, echoCancellation = false } = features;

    return { normalization, echoCancellation };
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
