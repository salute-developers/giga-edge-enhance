import { ModelsTmpl } from './common';

export const WASM_URL = new URL('../../generated/wasm/gigaedge-spench.wasm', import.meta.url);
export const WASM_DSP_URL = new URL('../../generated/wasm/gigaedge-spench.dsp.wasm', import.meta.url);

const DENOISE_FAST_8_MODEL_URL = new URL('../../models/fast8.ges', import.meta.url);
const DENOISE_QUALITY_48_MODEL_URL = new URL('../../models/quality48.ges', import.meta.url);

export const DEFAULT_MODELS = {
    noiseCancellation: {
        fast8: DENOISE_FAST_8_MODEL_URL,
        quality48: DENOISE_QUALITY_48_MODEL_URL,
    },
} as const;

export function getDefaultModels<M extends ModelsTmpl>(models: Partial<M>): typeof DEFAULT_MODELS & M {
    return {
        noiseCancellation: { ...DEFAULT_MODELS.noiseCancellation, ...models.noiseCancellation },
    } as typeof DEFAULT_MODELS & M;
}
