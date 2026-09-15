import GigaDenoiser, { type PackedModelReader, type MainModule as GigaDenoiserCore } from '../../../generated/wasm/giga-denoiser';
import GigaDenoiserDsp, { type MainModule as GigaDenoiserDSPCore } from '../../../generated/wasm/giga-denoiser.dsp';
import {
    FEATURE_SWITCH_CACHE_MILLISECONDS,
    getDefaultAECConfig,
    getDefaultNormalizationConfig,
    HEAP_SIZE,
} from '../../utils/constants';
import { Logger } from '../../utils/logger';
import { AbstractGigaDenoiserWrapper } from '../../utils/wrapper';
import getHashCode from '../../utils/hash';
import { ModelKey, Settings } from '../../utils/common';
import GigaDenoiserError from '../../utils/error';

import Loader, { LoadError, ModelNotFoundError } from './loader';

export class InternalError extends GigaDenoiserError {
    override get name() {
        return 'InternalError';
    }
}

interface CreateGigaDenoiserWrapperParams {
    sampleRate: number;
    logger: Logger;
    loader: Loader;
    settings?: Settings;
    onHeavyLoadStart?: () => void;
    onHeavyLoadEnd?: () => void;
}

export class GigaDenoiserDspWrapper extends AbstractGigaDenoiserWrapper {
    protected bridge: GigaDenoiserDSPCore;

    protected dataCapacity = HEAP_SIZE;

    protected loopbackCapacity = HEAP_SIZE;

    protected constructor(
        wasm: ArrayBuffer,
        protected readonly sampleRate: number,
        protected readonly logger: Logger,
        protected readonly settings: Settings,
    ) {
        super();

        this.bridge = GigaDenoiserDsp({ wasmBinary: wasm, print: logger.debug, printErr: logger.error });
        this.dataPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
        if (!this.dataPtr) throw new InternalError('Could not allocate the WASM input buffer');
        try {
            this.lbPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
            if (!this.lbPtr) throw new InternalError('Could not allocate the WASM loopback buffer');
        } catch (error) {
            this.bridge._free(this.dataPtr);
            throw error;
        }
    }

    protected get heap(): Float32Array {
        return this.bridge.HEAPF32;
    }

    protected ensureScratchCapacity(frameSize: number, loopbackSize: number): void {
        if (frameSize > this.dataCapacity) {
            const dataPtr = this.bridge._malloc(frameSize * Float32Array.BYTES_PER_ELEMENT);
            if (!dataPtr) throw new InternalError('Could not allocate the WASM input buffer');
            this.bridge._free(this.dataPtr);
            this.dataPtr = dataPtr;
            this.dataCapacity = frameSize;
        }
        if (loopbackSize > this.loopbackCapacity) {
            const lbPtr = this.bridge._malloc(loopbackSize * Float32Array.BYTES_PER_ELEMENT);
            if (!lbPtr) throw new InternalError('Could not allocate the WASM loopback buffer');
            this.bridge._free(this.lbPtr);
            this.lbPtr = lbPtr;
            this.loopbackCapacity = loopbackSize;
        }
    }

    static async create({
        sampleRate,
        logger,
        loader,
        settings,
    }: CreateGigaDenoiserWrapperParams): Promise<GigaDenoiserDspWrapper | InternalError | LoadError> {
        const wasm = await loader.loadDSPWasm();

        if (wasm instanceof LoadError) {
            return wasm;
        }

        try {
            return new GigaDenoiserDspWrapper(wasm, sampleRate, logger, settings!);
        } catch (e) {
            return new InternalError(e);
        }
    }

    addNormalizer(config?: string): void | InternalError {
        const hashCode = config ? getHashCode(config) : 'default';
        const key = `normalizer-${hashCode}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug(
            'Add normalizer to GigaDenoiserDSP Pipeline.',
            config ? `Using custom config:\n${config}.` : 'Using default config.',
        );

        try {
            this.pipeline.push([
                key,
                cached ||
                    new this.bridge.AudioProcessPipelineElement(
                        config || getDefaultNormalizationConfig(this.sampleRate, this.settings),
                    ),
            ]);
        } catch (e) {
            return new InternalError(e);
        }
    }

    addEchoCanceller(config?: string): void | InternalError {
        const hashCode = config ? getHashCode(config) : 'default';
        const key = `aec-${hashCode}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug(
            'Add aec to GigaDenoiserDSP Pipeline',
            config ? `Using custom config:\n${config}.` : 'Using default config.',
        );

        try {
            this.pipeline.push([
                key,
                cached || new this.bridge.EchoCancelationElement(config || getDefaultAECConfig(this.sampleRate)),
            ]);
        } catch (e) {
            return new InternalError(e);
        }
    }

    override pop() {
        const elem = this.pipeline.pop();

        if (!elem) return false;

        const [key, element] = elem;
        const timeout = setTimeout(() => {
            try {
                this.cache.get(key)?.[1].delete();
            } catch {}
            this.cache.delete(key);
        }, FEATURE_SWITCH_CACHE_MILLISECONDS);
        this.cache.set(key, [() => clearTimeout(timeout), element]);

        return true;
    }

    destroy() {
        super.destroy();

        this.bridge._free(this.dataPtr);
        this.bridge._free(this.lbPtr);
    }
}

export class GigaDenoiserWrapper extends AbstractGigaDenoiserWrapper {
    protected bridge: GigaDenoiserCore;

    protected dataCapacity = HEAP_SIZE;

    protected constructor(
        wasm: ArrayBuffer,
        protected readonly sampleRate: number,
        protected readonly logger: Logger,
        protected readonly loader: Loader,
        protected readonly onHeavyLoadStart: () => void,
        protected readonly onHeavyLoadEnd: () => void,
    ) {
        super();
        onHeavyLoadStart();
        try {
            this.bridge = GigaDenoiser({
                wasmBinary: wasm,
                print: logger.debug,
                printErr: logger.error,
            });
            this.dataPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
            if (!this.dataPtr) throw new InternalError('Could not allocate the WASM input buffer');
        } finally {
            onHeavyLoadEnd();
        }
    }

    protected get heap(): Float32Array {
        return this.bridge.HEAPF32;
    }

    protected ensureScratchCapacity(frameSize: number): void {
        if (frameSize <= this.dataCapacity) return;

        const dataPtr = this.bridge._malloc(frameSize * Float32Array.BYTES_PER_ELEMENT);
        if (!dataPtr) throw new InternalError('Could not allocate the WASM input buffer');
        this.bridge._free(this.dataPtr);
        this.dataPtr = dataPtr;
        this.dataCapacity = frameSize;
    }

    static async create({
        sampleRate,
        logger,
        loader,
        onHeavyLoadStart,
        onHeavyLoadEnd,
    }: CreateGigaDenoiserWrapperParams): Promise<GigaDenoiserWrapper | InternalError | LoadError> {
        const wasm = await loader.loadWasm();

        if (wasm instanceof LoadError) {
            return wasm;
        }

        try {
            return new GigaDenoiserWrapper(wasm, sampleRate, logger, loader, onHeavyLoadStart!, onHeavyLoadEnd!);
        } catch (e) {
            return new InternalError(e);
        }
    }

    override pop() {
        const elem = this.pipeline.pop();

        if (!elem) return false;

        const [key, element] = elem;
        const timeout = setTimeout(() => {
            try {
                this.cache.get(key)?.[1].delete();
            } catch {}
            this.cache.delete(key);
        }, FEATURE_SWITCH_CACHE_MILLISECONDS);
        this.cache.set(key, [() => clearTimeout(timeout), element]);

        return true;
    }

    async addNoiseCanceller(type: ModelKey): Promise<void | LoadError | ModelNotFoundError> {
        const key = `denoiser-${type}`;
        const cached = this.tryGetFromCache(key);

        if (cached) {
            this.pipeline.push([key, cached]);
        } else {
            this.logger.debug('model loading');
            const loadedModel = await this.loader.loadNoiseCancellationModel(type);

            if (loadedModel instanceof GigaDenoiserError) {
                return loadedModel;
            }
            this.logger.debug('model loaded');

            this.onHeavyLoadStart();
            try {
                const model = this.loadModel(loadedModel);

                if (model instanceof GigaDenoiserError) {
                    return model;
                }
                this.logger.debug('model created');

                try {
                    this.pipeline.push([key, new this.bridge.DenoisePipelineElement(this.sampleRate, model)]);
                    this.logger.debug('element created');
                } catch (e) {
                    return new InternalError(e);
                } finally {
                    model.delete();
                }
            } finally {
                this.onHeavyLoadEnd();
            }
        }

        this.logger.debug(`Add noise canceller to GigaDenoiser Pipeline. Model type: ${type}${cached ? ' (cached)' : ''}.`);
    }

    destroy() {
        super.destroy();

        this.bridge?._free(this.dataPtr);
    }

    protected loadModel(model: ArrayBuffer): PackedModelReader | InternalError {
        const modelU8 = new Uint8Array(model);
        const dataPtr = this.bridge._malloc(modelU8.length * Uint8Array.BYTES_PER_ELEMENT);
        (this.bridge.HEAPU8 as Uint8Array).set(modelU8, dataPtr);

        try {
            return new this.bridge.PackedModelReader(dataPtr, modelU8.length);
        } catch (e) {
            this.logger.error('Could not load model.');

            return new InternalError(e);
        } finally {
            this.bridge._free(dataPtr);
        }
    }
}
