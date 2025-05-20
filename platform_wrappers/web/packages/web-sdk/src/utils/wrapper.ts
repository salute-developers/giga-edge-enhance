import Spench, { MainModule as SpenchCore } from '../../lib/spench';
import SpenchDsp, { MainModule as SpenchDSPCore } from '../../lib/spench.dsp';

import { assert } from './error';
import { FEATURE_SWITCH_CACHE_MILLISECONDS, HEAP_SIZE } from './constants';
import { Logger } from './logger';

interface PipelineElement {
    process(srcPtr: number, chunkSize: number): number;
    process(srcPtr: number, loopbackPtr: number, chunkSize: number): number;
    resetContext(): void;
}

type Element = [cacheKey: string, PipelineElement];

abstract class AbstractSpenchWrapper {
    protected readonly pipeline: Element[] = [];

    protected dataPtr = 0;

    protected readonly cache: Map<string, [() => void, PipelineElement]> = new Map();

    protected get dataStartIndex() {
        return this.dataPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    protected abstract get heap(): Float32Array;

    pop() {
        const elem = this.pipeline.pop();

        if (!elem) return false;

        const [key, element] = elem;
        const timeout = setTimeout(() => this.cache.delete(key), FEATURE_SWITCH_CACHE_MILLISECONDS);
        this.cache.set(key, [() => clearTimeout(timeout), element]);

        return true;
    }

    clear() {
        while (this.pop());
    }

    process(frame: Float32Array): Float32Array {
        if (!this.pipeline.length) {
            return new Float32Array(frame.subarray());
        }

        this.heap.set(frame, this.dataStartIndex);
        let samplesProcessed = frame.length;

        for (const [, el] of this.pipeline) {
            samplesProcessed = el.process(this.dataPtr, samplesProcessed);

            if (!samplesProcessed) break;
        }

        return new Float32Array(this.heap.subarray(this.dataStartIndex, this.dataStartIndex + samplesProcessed));
    }

    resetContext() {
        for (const [, el] of this.pipeline) {
            el.resetContext();
        }
    }

    protected tryGetFromCache(key: string): PipelineElement | undefined {
        const res = this.cache.get(key);

        if (!res) return;
        this.cache.delete(key);
        const [clear, element] = res;
        clear();

        return element;
    }

    abstract destroy(): void;
}

export class SpenchDspWrapper extends AbstractSpenchWrapper {
    protected static bridge: SpenchDSPCore | undefined;

    constructor(
        protected readonly sampleRate: number,
        protected logger: Logger,
    ) {
        super();
        assert(SpenchDspWrapper.bridge, 'Bridge must be created');

        this.dataPtr = SpenchDspWrapper.bridge!._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
    }

    protected get heap(): Float32Array {
        return SpenchDspWrapper.bridge!.HEAPF32;
    }

    static createBridge(wasm: ArrayBuffer): void {
        if (SpenchDspWrapper.bridge) {
            return;
        }

        SpenchDspWrapper.bridge = SpenchDsp({ wasmBinary: wasm });
    }

    addNormalizer(): void {
        const key = 'normalizer';

        this.logger.debug('add normalizer to SpenchDSP Pipeline');
        this.pipeline.push([key, new SpenchDspWrapper.bridge!.AudioProcessPipelineElement(this.sampleRate, '')]);
    }

    override pop() {
        const elem = this.pipeline.pop();

        return Boolean(elem);
    }

    destroy() {
        SpenchDspWrapper.bridge?._free(this.dataPtr);
    }
}

export class SpenchWrapper extends AbstractSpenchWrapper {
    protected static bridge: SpenchCore | undefined;

    constructor(
        protected readonly sampleRate: number,
        protected logger: Logger,
    ) {
        super();
        assert(SpenchWrapper.bridge, 'Bridge must be created');

        this.dataPtr = SpenchWrapper.bridge!._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
    }

    protected get heap(): Float32Array {
        return SpenchWrapper.bridge!.HEAPF32;
    }

    static createBridge(wasm: ArrayBuffer, data: ArrayBuffer): void {
        if (SpenchWrapper.bridge) {
            return;
        }

        SpenchWrapper.bridge = Spench({
            wasmBinary: wasm,
            locateFile: (_s1: string, _s2: string) => {
                return '';
            },
            getPreloadedPackage(_remotePackageName: string, _remotePackageSize: number) {
                return data;
            },
        });
    }

    addNormalizer(): void {
        const key = 'normalizer';

        this.logger.debug('add normalizer to Spench Pipeline');
        this.pipeline.push([key, new SpenchWrapper.bridge!.AudioProcessPipelineElement(this.sampleRate, '')]);
    }

    addDenoiser(configPath: string): void {
        const key = `denoiser-${configPath}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug('add denoiser to Spench Pipeline', configPath);
        this.pipeline.push([
            key,
            cached || new SpenchWrapper.bridge!.DenoisePipelineElement(this.sampleRate, configPath),
        ]);
    }

    addDereverberator(configPath: string): void {
        const key = `dereverb-${configPath}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug('add dereverberator to Spench Pipeline', configPath);
        this.pipeline.push([
            key,
            cached || new SpenchWrapper.bridge!.DereverbPipelineElement(this.sampleRate, configPath),
        ]);
    }

    destroy() {
        SpenchWrapper.bridge?._free(this.dataPtr);
    }
}
