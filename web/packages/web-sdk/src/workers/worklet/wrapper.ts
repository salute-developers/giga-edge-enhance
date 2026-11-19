import GigaEdgeEnhanceDsp, { MainModule as GigaEdgeEnhanceDSPCore } from '../../../generated/wasm/gigaedge-enhance.dsp';
import {
    FEATURE_SWITCH_CACHE_MILLISECONDS,
    getDefaultAECConfig,
    getDefaultNormalizationConfig,
    HEAP_SIZE,
} from '../../utils/constants';
import { Logger } from '../../utils/logger';
import getHashCode from '../../utils/hash';
import { AbstractGigaEdgeEnhanceWrapper } from '../../utils/wrapper';
import { Settings } from '../../utils/common';

export class GigaEdgeEnhanceDspWorkletWrapper extends AbstractGigaEdgeEnhanceWrapper {
    protected bridge: GigaEdgeEnhanceDSPCore;

    protected timeouts: Map<string, number> = new Map();

    constructor(
        wasm: ArrayBuffer,
        protected readonly sampleRate: number,
        protected readonly logger: Logger,
        protected readonly settings: Settings,
    ) {
        super();

        this.bridge = GigaEdgeEnhanceDsp({ wasmBinary: wasm, print: logger.debug, printErr: logger.error });
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
            'add normalizer to GigaEdgeEnhanceDSP Pipeline.',
            config ? `using custom config:\n${config}` : 'using default config',
        );
        this.pipeline.push([
            key,
            cached ||
                new this.bridge.AudioProcessPipelineElement(
                    config || getDefaultNormalizationConfig(this.sampleRate, this.settings),
                ),
        ]);
    }

    addEchoCanceller(config?: string): void {
        const hashCode = config ? getHashCode(config) : 'default';
        const key = `aec-${hashCode}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug(
            'add aec to GigaEdgeEnhanceDSP Pipeline',
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
                this.logger.debug(`Cleared ${key} from GigaEdgeEnhanceDSP cache`);

                try {
                    this.cache.get(key)?.[1].delete();
                } catch {}
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
