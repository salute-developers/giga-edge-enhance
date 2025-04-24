type ModelOrigin = string;

export type Models = {
    noiseCancellation: { full24k: ModelOrigin; lite24k: ModelOrigin };
    dereverberation: { full48k: ModelOrigin };
};

export type Features = Partial<{
    noiseCancellation: boolean | keyof Models['noiseCancellation'];
    dereverberation: boolean | keyof Models['dereverberation'];
    normalization: boolean | 'rc' | 'agc';
    echoCancellation: boolean;
}>;

export type SharedFeatures = Pick<Features, 'normalization'>;

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
