export interface TypedMessageEvent<DType> extends MessageEvent {
    data: DType;
}

// TODO retries (use ky), abort on
export async function downloadFileAsBuffer(url: URL): Promise<[ArrayBuffer, number]> {
    const start = performance.now();

    const response = await fetch(url);
    const arrayBuffer = await response.arrayBuffer();

    const elapsed = Math.round(performance.now() - start);

    return [arrayBuffer, elapsed];
}
