import Spench, { type PackedModelReader, type MainModule as SpenchCore } from '../../../lib/spench';
import SpenchDsp, { type MainModule as SpenchDSPCore } from '../../../lib/spench.dsp';
import {
    FEATURE_SWITCH_CACHE_MILLISECONDS,
    getDefaultAECConfig,
    getDefaultNormalizationConfig,
    HEAP_SIZE,
} from '../../utils/constants';
import { Logger } from '../../utils/logger';
import { AbstractSpenchWrapper } from '../../utils/wrapper';
import getHashCode from '../../utils/hash';
import { ModelKey } from '../../utils/common';
import SpenchError from '../../utils/error';

import Loader, { LoadError, ModelNotFoundError } from './loader';

export class InternalError extends SpenchError {}

interface CreateSpenchWrapperParams {
    sampleRate: number;
    logger: Logger;
    loader: Loader;
    onHeavyLoadStart?: () => void;
    onHeavyLoadEnd?: () => void;
}

export class SpenchDspWrapper extends AbstractSpenchWrapper {
    protected bridge: SpenchDSPCore;

    protected constructor(
        wasm: ArrayBuffer,
        protected readonly sampleRate: number,
        protected readonly logger: Logger,
    ) {
        super();

        this.bridge = SpenchDsp({ wasmBinary: wasm, print: logger.debug, printErr: logger.error });
        this.dataPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
        this.lbPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
    }

    protected get heap(): Float32Array {
        return this.bridge.HEAPF32;
    }

    static async create({
        sampleRate,
        logger,
        loader,
    }: CreateSpenchWrapperParams): Promise<SpenchDspWrapper | InternalError | LoadError> {
        const wasm = await loader.loadDSPWasm();

        if (wasm instanceof LoadError) {
            return wasm;
        }

        try {
            return new SpenchDspWrapper(wasm, sampleRate, logger);
        } catch (e) {
            return new InternalError(e);
        }
    }

    addNormalizer(config?: string): void | InternalError {
        const hashCode = config ? getHashCode(config) : 'default';
        const key = `normalizer-${hashCode}`;
        const cached = this.tryGetFromCache(key);

        this.logger.debug(
            'Add normalizer to SpenchDSP Pipeline.',
            config ? `Using custom config:\n${config}.` : 'Using default config.',
        );

        try {
            this.pipeline.push([
                key,
                cached ||
                    new this.bridge.AudioProcessPipelineElement(
                        config || getDefaultNormalizationConfig(this.sampleRate),
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
            'Add aec to SpenchDSP Pipeline',
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

export class SpenchWrapper extends AbstractSpenchWrapper {
    protected bridge: SpenchCore;

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
        this.bridge = Spench({
            wasmBinary: wasm,
            print: logger.debug,
            printErr: logger.error,
        });
        this.dataPtr = this.bridge._malloc(HEAP_SIZE * Float32Array.BYTES_PER_ELEMENT);
        onHeavyLoadEnd();
    }

    protected get heap(): Float32Array {
        return this.bridge.HEAPF32;
    }

    static async create({
        sampleRate,
        logger,
        loader,
        onHeavyLoadStart,
        onHeavyLoadEnd,
    }: CreateSpenchWrapperParams): Promise<SpenchWrapper | InternalError | LoadError> {
        const wasm = await loader.loadWasm();

        if (wasm instanceof LoadError) {
            return wasm;
        }

        try {
            return new SpenchWrapper(wasm, sampleRate, logger, loader, onHeavyLoadStart!, onHeavyLoadEnd!);
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

            if (loadedModel instanceof SpenchError) {
                return loadedModel;
            }
            this.logger.debug('model loaded');

            this.onHeavyLoadStart();
            const model = this.loadModel(loadedModel);

            if (model instanceof SpenchError) {
                this.onHeavyLoadEnd();

                return model;
            }
            this.logger.debug('model created');

            try {
                this.pipeline.push([key, new this.bridge.DenoisePipelineElement(this.sampleRate, model)]);
                this.onHeavyLoadEnd();
                this.logger.debug('element created');
            } catch (e) {
                return new InternalError(e);
            } finally {
                model.delete();
            }
        }

        this.logger.debug(`Add noise canceller to Spench Pipeline. Model type: ${type}${cached ? ' (cached)' : ''}.`);
    }

    async addDereverberator(type: ModelKey): Promise<void | LoadError | ModelNotFoundError> {
        const key = `dereverb-${type}`;
        const cached = this.tryGetFromCache(key);

        if (cached) {
            this.pipeline.push([key, cached]);
        } else {
            const loadedModel = await this.loader.loadDereverberationModel(type);

            if (loadedModel instanceof SpenchError) {
                return loadedModel;
            }

            this.onHeavyLoadStart();
            const model = this.loadModel(loadedModel);

            if (model instanceof SpenchError) {
                this.onHeavyLoadEnd();

                return model;
            }

            try {
                this.pipeline.push([key, new this.bridge.DereverbPipelineElement(this.sampleRate, model)]);
                this.onHeavyLoadEnd();
            } catch (e) {
                return new InternalError(e);
            } finally {
                model.delete();
            }
        }

        this.logger.debug(`Add dereverberator to Spench Pipeline. Model type: ${type}${cached ? ' (cached)' : ''}.`);
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
            const modelReader = new this.bridge.PackedModelReader(dataPtr, modelU8.length);

            this.bridge._free(dataPtr);

            return modelReader;
        } catch (e) {
            this.logger.error('Could not load model.');

            return new InternalError(e);
        }
    }
}
