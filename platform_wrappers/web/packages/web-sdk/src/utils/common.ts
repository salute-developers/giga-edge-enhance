type ModelOrigin = string;

export type Models = {
    noiseCancellation: { full24k: ModelOrigin; lite24k: ModelOrigin };
    echoCancellation: { full48k: ModelOrigin };
};

export type Features = Partial<{
    noiseCancellation: boolean | keyof Models['noiseCancellation'];
    echoCancellation: boolean | keyof Models['echoCancellation'];
    normalization: boolean;
}>;

export type SharedFeatures = Pick<Features, 'normalization'>;

// TODO retries (use ky), abort on
export async function downloadFileAsBuffer(url: URL): Promise<[ArrayBuffer, number]> {
    const start = performance.now();

    const response = await fetch(url);
    const arrayBuffer = await response.arrayBuffer();

    const elapsed = Math.round(performance.now() - start);

    return [arrayBuffer, elapsed];
}
