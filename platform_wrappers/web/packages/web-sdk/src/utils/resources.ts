export const workerUrl = new URL('../../dist/workers/spench-worker.min.js', import.meta.url).href;
export const workletUrl = new URL('../../dist/workers/spench-worklet.min.js', import.meta.url).href;
export const workletDspUrl = new URL('../../dist/workers/spench-worklet-dsp.min.js', import.meta.url).href;

export const wasmUrl = new URL('../../bin/spench.wasm', import.meta.url).href;
export const wasmDspUrl = new URL('../../bin/spench.dsp.wasm', import.meta.url).href;

export const dataUrl = new URL('../../bin/spench.data', import.meta.url).href;
