import Spench, { MainModule as SpenchCore } from '../../lib/spench';
import SpenchDsp, { MainModule as SpenchDSPCore } from '../../lib/spench.dsp';

import { assert } from './error';
import {
    FEATURE_SWITCH_CACHE_MILLISECONDS,
    getDefaultAECConfig,
    getDefaultNormalizationConfig,
    HEAP_SIZE,
} from './constants';
import { Logger } from './logger';
import getHashCode from './hash';

interface AbstractPipelineElement {
    resetContext(): void;
    delete(): void;
}

interface CommonPipelineElement extends AbstractPipelineElement {
    process(srcPtr: number, chunkSize: number): number;
}

interface AECElement extends AbstractPipelineElement {
    process(srcPtr: number, lbPtr: number, chunkSize: number): number;
}

type PipelineElement = CommonPipelineElement | AECElement;

type Element = [cacheKey: string, PipelineElement];

abstract class AbstractSpenchWrapper {
    protected readonly pipeline: Element[] = [];

    protected dataPtr = 0;

    protected lbPtr = 0;

    protected readonly cache: Map<string, [() => void, PipelineElement]> = new Map();

    protected get dataStartIndex() {
        return this.dataPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    protected get lbStartIndex() {
        return this.lbPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    protected abstract get heap(): Float32Array;

    clear() {
        while (this.pop());
    }

    process(frame: Float32Array, loopback?: Float32Array): Float32Array {
        if (!this.pipeline.length) {
            return new Float32Array(frame.subarray());
        }

        this.heap.set(frame, this.dataStartIndex);

        if (loopback) {
            this.heap.set(loopback, this.lbStartIndex);
        }

        let samplesProcessed = frame.length;

        for (const [, el] of this.pipeline) {
            if (el.process.length === 2) {
                samplesProcessed = (el as CommonPipelineElement).process(this.dataPtr, samplesProcessed);
            } else if (el.process.length === 3 && loopback) {
                samplesProcessed = (el as AECElement).process(this.dataPtr, this.lbPtr, samplesProcessed);
            }

            if (!samplesProcessed) break;
        }

        return new Float32Array(this.heap.subarray(this.dataStartIndex, this.dataStartIndex + samplesProcessed));
    }

    resetContext() {
        for (const [, el] of this.pipeline) {
            el.resetContext();
        }
    }

    destroy(): void {
        let current;
        while ((current = this.pipeline.pop())) {
            const [, element] = current;
            element.delete();
        }
    }

    protected tryGetFromCache(key: string): PipelineElement | undefined {
        const res = this.cache.get(key);

        if (!res) return;
        this.cache.delete(key);
        const [clear, element] = res;
        clear();
        element.resetContext();

        return element;
    }

    protected abstract pop(): boolean;
}

export class SpenchDspWrapper extends AbstractSpenchWrapper {
    protected bridge: SpenchDSPCore;

    protected timeouts: Map<string, number> = new Map();

    constructor(
        wasm: ArrayBuffer,
        protected readonly sampleRate: number,
        protected logger: Logger,
    ) {
        super();

        this.bridge = SpenchDsp({ wasmBinary: wasm, print: logger.info, printErr: logger.error });
        this.dataPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
        this.lbPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
    }

    protected get heap(): Float32Array {
        return this.bridge.HEAPF32;
    }

    addNormalizer(config?: string): void {
        const hashCode = config ? getHashCode(config) : 'default';
        const key = `normalizer-${hashCode}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug(
            'add normalizer to SpenchDSP Pipeline.',
            config ? `using custom config:\n${config}` : 'using default config',
        );
        this.pipeline.push([
            key,
            cached ||
                new this.bridge.AudioProcessPipelineElement(config || getDefaultNormalizationConfig(this.sampleRate)),
        ]);
    }

    addEchoCanceller(config?: string): void {
        const hashCode = config ? getHashCode(config) : 'default';
        const key = `aec-${hashCode}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug(
            'add aec to SpenchDSP Pipeline',
            config ? `using custom config:\n${config}` : 'using default config',
        );
        this.pipeline.push([
            key,
            cached || new this.bridge.EchoCancelationElement(config || getDefaultAECConfig(this.sampleRate)),
        ]);
    }

    override pop() {
        const elem = this.pipeline.pop();

        if (!elem) return false;

        const [key, element] = elem;

        this.timeouts.set(key, currentTime);
        this.cache.set(key, [() => this.timeouts.delete(key), element]);

        return true;
    }

    tick() {
        for (const [key, time] of this.timeouts) {
            if ((currentTime - time) * 1000 > FEATURE_SWITCH_CACHE_MILLISECONDS) {
                this.logger.debug(`Cleared ${key} from SpenchDSP cache`);

                this.cache.get(key)?.[1].delete();
                this.cache.delete(key);
                this.timeouts.delete(key);
            }
        }
    }

    destroy() {
        super.destroy();

        this.bridge._free(this.dataPtr);
        this.bridge._free(this.lbPtr);
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

    static createBridge(wasm: ArrayBuffer, data: ArrayBuffer, logger: Logger): void {
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
            print: logger.debug,
            printErr: logger.error,
        });
    }

    override pop() {
        const elem = this.pipeline.pop();

        if (!elem) return false;

        const [key, element] = elem;
        const timeout = setTimeout(() => {
            this.cache.get(key)?.[1].delete();
            this.cache.delete(key);
        }, FEATURE_SWITCH_CACHE_MILLISECONDS);
        this.cache.set(key, [() => clearTimeout(timeout), element]);

        return true;
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
        super.destroy();

        SpenchWrapper.bridge?._free(this.dataPtr);
    }
}
