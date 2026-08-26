import { ModelsTmpl } from './common';

export const WASM_URL = new URL('../../bin/spench.wasm', import.meta.url);
export const WASM_DSP_URL = new URL('../../bin/spench.dsp.wasm', import.meta.url);

const DENOISE_QUANT_MODEL_URL = new URL('../../models/fast_model_int8.spench', import.meta.url);
const DENOISE_QUANT_MODEL_8K_URL = new URL('../../models/fast_model_int8_8k.spench', import.meta.url);
const DENOISE_MOBILE_MODEL_URL = new URL('../../models/mobile_group8_hybrid_loss_fp32.spench', import.meta.url);

export const DEFAULT_MODELS = {
    noiseCancellation: {
        lite8k: DENOISE_QUANT_MODEL_8K_URL,
        lite24k: DENOISE_QUANT_MODEL_URL,
        full24k: DENOISE_MOBILE_MODEL_URL,
    },
} as const;

export function getDefaultModels<M extends ModelsTmpl>(models: Partial<M>): typeof DEFAULT_MODELS & M {
    return {
        noiseCancellation: { ...DEFAULT_MODELS.noiseCancellation, ...models.noiseCancellation },
    } as typeof DEFAULT_MODELS & M;
}
