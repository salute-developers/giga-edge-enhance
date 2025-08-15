import { DEFAULT_LP_MAX_REFILL_TIMES, DEFAULT_LP_WINDOW_SECONDS } from './constants';

type ModelOrigin = string;

export type Models = {
    noiseCancellation: { full24k: ModelOrigin; lite24k: ModelOrigin };
    dereverberation: { full48k: ModelOrigin };
};

export type WorkerFeatures = {
    noiseCancellation: boolean | keyof Models['noiseCancellation'];
    dereverberation: boolean | keyof Models['dereverberation'];
};

export type WorkletFeatures = {
    normalization: boolean;
    echoCancellation: boolean;
};

export type Features = WorkerFeatures & WorkletFeatures;

export type Settings = {
    lowPerformanceWindowSeconds: number;
    lowPerformanceMaxRefillTimes: number;
};

export function getSettingsWithDefaults(settings: Partial<Settings> = {}): Settings {
    return {
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

export function areWorkerFeaturesEnabled(features: Partial<Features>) {
    return Object.values(getWorkerFeatures(features)).some(Boolean);
}

export function areWorkletFeaturesEnabled(features: Partial<Features>) {
    return Object.values(getWorkletFeatures(features)).some(Boolean);
}

export function getWorkerFeatures(features: Partial<Features> = {}): WorkerFeatures {
    const { noiseCancellation = false, dereverberation = false } = features;

    return { noiseCancellation, dereverberation };
}

export function getWorkletFeatures(features: Partial<Features> = {}): WorkletFeatures {
    const { normalization = false, echoCancellation = false } = features;

    return { normalization, echoCancellation };
}

export async function downloadFileAsBuffer(url: URL): Promise<[ArrayBuffer, number]> {
    const start = performance.now();

    const response = await fetch(url);
    const arrayBuffer = await response.arrayBuffer();

    const elapsed = Math.round(performance.now() - start);

    return [arrayBuffer, elapsed];
}

export async function retry<E extends Error, R>(fn: () => Promise<E | R>, retries = 3): Promise<E | R> {
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
