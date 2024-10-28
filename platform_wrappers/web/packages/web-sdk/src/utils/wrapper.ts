import Spench, { AudioProcessPipeline, DenoisePipeline, SpenchWasm } from 'wasm-wrapper';

import { assert } from './error';
import { SPENCH_FRAME_SIZE_SECONDS, SPENCH_NORM_FRAME_SIZE } from './constants';

type InstanceType = 'denoiser' | 'normalizer';

// TODO serialize model and load from the SDK
const WASM_VIRTUALFS_MODEL_PATH = 'data/config/mobile_group8_hybrid_loss_fp32.yaml';

class SpenchWrapper {
    static #bridge: SpenchWasm | undefined;

    readonly frameSize: number;

    readonly #instance: DenoisePipeline | AudioProcessPipeline;

    readonly #heap: Float32Array;

    readonly #inputPtr: number;

    readonly #outputPtr: number;

    constructor(type: InstanceType, sampleRate: number) {
        assert(SpenchWrapper.#bridge, 'Bridge should be created');

        if (type === 'denoiser') {
            this.#instance = new SpenchWrapper.#bridge.DenoisePipeline(sampleRate, WASM_VIRTUALFS_MODEL_PATH);
            this.frameSize = Math.round(SPENCH_FRAME_SIZE_SECONDS * sampleRate);
        } else if (type === 'normalizer') {
            this.#instance = new SpenchWrapper.#bridge.AudioProcessPipeline(sampleRate);
            this.frameSize = SPENCH_NORM_FRAME_SIZE;
        } else {
            assert(false, 'Unknown instance type');
        }

        this.#heap = SpenchWrapper.#bridge.HEAPF32;
        this.#inputPtr = SpenchWrapper.#bridge._malloc(this.frameSize * Float32Array.BYTES_PER_ELEMENT);
        this.#outputPtr = SpenchWrapper.#bridge._malloc(this.frameSize * Float32Array.BYTES_PER_ELEMENT);
    }

    get #inputStartIndex() {
        return this.#inputPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    get #outputStartIndex() {
        return this.#outputPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    static createBridge(wasm: ArrayBuffer, data?: ArrayBuffer): void {
        if (SpenchWrapper.#bridge) {
            return;
        }

        SpenchWrapper.#bridge = Spench({
            wasmBinary: wasm,
            locateFile: (_s1: string, _s2: string) => {
                return '';
            },
            getPreloadedPackage(_remotePackageName: string, _remotePackageSize: number) {
                return data || new ArrayBuffer(0);
            },
        });
    }

    process(frame: Float32Array, output?: Float32Array): void {
        assert(frame.length === this.frameSize, 'Frame size and input buffer length must match');
        this.#heap.set(frame, this.#inputStartIndex);
        this.#instance.process(this.#inputPtr, this.#outputPtr, this.frameSize);

        const out = output || frame;
        out.set(this.#heap.subarray(this.#outputStartIndex, this.#outputStartIndex + this.frameSize));
    }

    resetContext() {
        assert(
            this.#instance instanceof SpenchWrapper.#bridge!.DenoisePipeline,
            'Only denoiser has resettable context',
        );

        this.#instance.resetContext();
    }

    destroy() {
        SpenchWrapper.#bridge?._free(this.#inputPtr);
        SpenchWrapper.#bridge?._free(this.#outputPtr);
    }
}

export default SpenchWrapper;
