export type Logger = {
    debug: (...data: unknown[]) => void;
    error: (...data: unknown[]) => void;
};

export type WebDenoiserPerformanceMonitor = {
    chunkProcessTimeHistogram: {
        addValue: (value: number) => void;
    };
    meanChunkCountPerSecond: {
        tick: () => void;
        start: () => void;
        stop: () => void;
    };
};

export type FeatureMetrics = {
    reportDenoiserWasmDownloadTime: (elapsedMs: number) => void;
    reportDenoiserWasmDataDownloadTime: (elapsedMs: number) => void;
    reportDenoiserRuntimeInitTime: (elapsedMs: number) => void;
};

export async function downloadFileAsBuffer(url: string): Promise<[ArrayBuffer, number]> {
    const start = performance.now();

    const response = await fetch(url);
    const arrayBuffer = await response.arrayBuffer();

    const elapsed = Math.round(performance.now() - start);

    return [arrayBuffer, elapsed];
}
