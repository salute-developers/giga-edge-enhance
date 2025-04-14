import Spench, { PipelineElement, SpenchWasm } from 'wasm-wrapper';

import SpenchError, { assert } from './error';
import { FEATURE_SWITCH_CACHE_MILLISECONDS, HEAP_SIZE } from './constants';
import { Logger } from './logger';

class NotSupportedError extends SpenchError {}

type Element = [cacheKey: string, PipelineElement];

class SpenchWrapper {
    static isNormalizationMode: boolean = false;

    static #bridge: SpenchWasm | undefined;

    readonly #pipeline: Element[] = [];

    #dataPtr = 0;

    readonly #cache: Map<string, [() => void, PipelineElement]> = new Map();

    constructor(
        private readonly sampleRate: number,
        private readonly logger: Logger,
    ) {
        assert(SpenchWrapper.#bridge, 'Bridge should be created');

        this.#dataPtr = SpenchWrapper.#bridge!._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
    }

    get #dataStartIndex() {
        return this.#dataPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    get #heap(): Float32Array {
        return SpenchWrapper.#bridge!.HEAPF32;
    }

    static createBridge(wasm: ArrayBuffer, data?: ArrayBuffer): void {
        if (SpenchWrapper.#bridge) {
            return;
        }

        if (!data) {
            SpenchWrapper.isNormalizationMode = true;
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

    addNormalizer(): void {
        const key = 'normalizer';
        const cached = this.#tryGetFromCache(key);

        this.logger.debug('add norm');
        this.#pipeline.push([
            key,
            cached || new SpenchWrapper.#bridge!.AudioProcessPipelineElement(this.sampleRate, ''),
        ]);
    }

    addDenoiser(configPath: string): void {
        if (SpenchWrapper.isNormalizationMode) {
            throw new NotSupportedError('Features other than normalization are disabled');
        }

        const key = `denoiser-${configPath}`;
        const cached = this.#tryGetFromCache(key);

        this.logger.debug('add denoiser', configPath);
        this.#pipeline.push([
            key,
            cached || new SpenchWrapper.#bridge!.DenoisePipelineElement(this.sampleRate, configPath),
        ]);
    }

    addDevererberator(configPath: string): void {
        if (SpenchWrapper.isNormalizationMode) {
            throw new NotSupportedError('Features other than normalization are disabled');
        }

        const key = `dereverb-${configPath}`;
        const cached = this.#tryGetFromCache(key);

        this.logger.debug('add dereverb', configPath);
        this.#pipeline.push([
            key,
            cached || new SpenchWrapper.#bridge!.DereverbPipelineElement(this.sampleRate, configPath),
        ]);
    }

    pop() {
        const elem = this.#pipeline.pop();

        if (!elem) return false;

        const [key, element] = elem;
        const timeout = setTimeout(() => this.#cache.delete(key), FEATURE_SWITCH_CACHE_MILLISECONDS);
        this.#cache.set(key, [() => clearTimeout(timeout), element]);

        return true;
    }

    clear() {
        while (this.pop());
    }

    process(frame: Float32Array): Float32Array {
        if (!this.#pipeline.length) {
            return new Float32Array(frame.subarray());
        }

        this.#heap.set(frame, this.#dataStartIndex);
        let samplesProcessed = frame.length;

        for (const [, el] of this.#pipeline) {
            samplesProcessed = el.process(this.#dataPtr, samplesProcessed);

            if (!samplesProcessed) break;
        }

        return new Float32Array(this.#heap.subarray(this.#dataStartIndex, this.#dataStartIndex + samplesProcessed));
    }

    resetContext() {
        for (const [, el] of this.#pipeline) {
            el.resetContext();
        }
    }

    destroy() {
        SpenchWrapper.#bridge?._free(this.#dataPtr);
    }

    #tryGetFromCache(key: string): PipelineElement | undefined {
        const res = this.#cache.get(key);

        if (!res) return;
        this.#cache.delete(key);
        const [clear, element] = res;
        clear();

        return element;
    }
}

export default SpenchWrapper;
