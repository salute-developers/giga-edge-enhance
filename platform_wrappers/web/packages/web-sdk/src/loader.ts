import SpenchError from './utils/error';
import { SpenchAudioNode, SpenchAudioNodeImpl } from './audioNode';
import { downloadFileAsBuffer, Features } from './utils/common';
import createLogger, { ExternalLogger, Logger } from './utils/logger';
import { createAudioNodeTelemetry, LoaderTelemetry, SendMetricFunction } from './utils/telemetry';

class CreateAudioNodeError extends SpenchError {}
class LoadError extends SpenchError {}

type CreateSpenchParams = {
    logger?: ExternalLogger;
    sendMetric?: SendMetricFunction;
    preloadFeatures?: Features;
    isDebug?: boolean;
};

class SpenchSDK {
    #ctx: AudioContext;

    #logger: Logger;

    #telemetry: LoaderTelemetry;

    #sendMetricFunction: SendMetricFunction | undefined;

    #wasmLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    #dataLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    #preloadedWorker: Worker | undefined;

    #workletLoadPromise: Promise<boolean | LoadError> | undefined;

    #isWorkletRegistered = false;

    #normalizerWorkletLoadPromise: Promise<boolean | LoadError> | undefined;

    #isNormalizerWorkletRegistered = false;

    protected constructor(ctx: AudioContext, logger: Logger, sendMetric: SendMetricFunction | undefined) {
        this.#ctx = ctx;
        this.#logger = logger;
        this.#sendMetricFunction = sendMetric;
        this.#telemetry = createAudioNodeTelemetry(logger, sendMetric);
    }

    static isEnvironmentSupported(): boolean {
        // TODO browser versions
        return true;
    }

    static create(
        ctx: AudioContext,
        { isDebug, logger: externalLogger, preloadFeatures, sendMetric }: CreateSpenchParams = {},
    ): SpenchSDK {
        const logger = createLogger(Boolean(isDebug), externalLogger);
        const instance = new SpenchSDK(ctx, logger, sendMetric);

        if (preloadFeatures) {
            instance.#preloadFeatures(preloadFeatures);
        }

        return instance;
    }

    async createAudioNode(features: Features = {}): Promise<SpenchAudioNode | CreateAudioNodeError> {
        const wasmPromise = Promise.resolve(this.#wasmLoadPromise).then((res) =>
            res instanceof ArrayBuffer ? res : this.#loadWasm(),
        );

        // Load normalizer worklet
        // TODO unify
        if (features.normalization && !features.noiseCancellation && !features.noiseCancellation) {
            const [wasm, normalizerWorkletResult] = await Promise.all([
                wasmPromise,
                Promise.resolve(this.#normalizerWorkletLoadPromise).then((res) =>
                    res === true ? res : this.#loadNormalizerWorklet(),
                ),
            ]);

            if (wasm instanceof LoadError || normalizerWorkletResult !== true) {
                return new CreateAudioNodeError();
            }

            // Can't reuse ArrayBuffer as it's transferred to the different context
            // Also binaries are quite big, so we rely on the browser cache
            this.#wasmLoadPromise = undefined;

            return new SpenchAudioNodeImpl({
                kind: 'normalizer',
                features: { normalization: true },
                ctx: this.#ctx,
                wasm,
                logger: this.#logger,
                telemetry: createAudioNodeTelemetry(this.#logger, this.#sendMetricFunction),
            });
        }

        const worker = this.#preloadedWorker || this.#createWorker();

        const [wasm, data, workletResult] = await Promise.all([
            wasmPromise,
            Promise.resolve(this.#dataLoadPromise).then((res) =>
                res instanceof ArrayBuffer ? res : this.#loadModelsData(),
            ),
            Promise.resolve(this.#workletLoadPromise).then((res) => (res === true ? res : this.#loadSpenchWorklet())),
        ]);

        if (wasm instanceof LoadError || data instanceof LoadError || workletResult !== true) {
            return new CreateAudioNodeError();
        }

        // Can't reuse ArrayBuffer as it's transferred to the different context
        // Also binaries are quite big, so we rely on the browser cache
        this.#wasmLoadPromise = undefined;
        this.#dataLoadPromise = undefined;

        return new SpenchAudioNodeImpl({
            kind: 'processor',
            features,
            ctx: this.#ctx,
            wasm,
            data,
            worker,
            logger: this.#logger,
            telemetry: createAudioNodeTelemetry(this.#logger, this.#sendMetricFunction),
        });
    }

    // TODO link rel prefetch / url list for preloading / maybe load by consumer app
    #preloadFeatures(features: Features): void {
        this.#wasmLoadPromise = this.#loadWasm();

        if (features.noiseCancellation || features.echoCancellation) {
            this.#dataLoadPromise = this.#loadModelsData();
            this.#preloadedWorker = this.#createWorker();
            this.#workletLoadPromise = this.#loadSpenchWorklet();

            return;
        }

        if (features.normalization) {
            this.#normalizerWorkletLoadPromise = this.#loadNormalizerWorklet();
        }
    }

    async #loadSpenchWorklet(): Promise<boolean | LoadError> {
        try {
            if (!this.#isWorkletRegistered) {
                await this.#ctx.audioWorklet.addModule(
                    new URL('../dist/workers/spench-worklet.min.js', import.meta.url),
                );
                this.#isWorkletRegistered = true;
            }

            return true;
        } catch {
            return new LoadError();
        }
    }

    async #loadNormalizerWorklet(): Promise<boolean | LoadError> {
        try {
            if (!this.#isNormalizerWorkletRegistered) {
                await this.#ctx.audioWorklet.addModule(
                    new URL('../dist/workers/normalizer-worklet.min.js', import.meta.url),
                );
                this.#isNormalizerWorkletRegistered = true;
            }

            return true;
        } catch {
            return new LoadError();
        }
    }

    async #loadWasm(): Promise<ArrayBuffer | LoadError> {
        try {
            const [buffer, time] = await downloadFileAsBuffer(new URL('../bin/Spench.wasm', import.meta.url));
            this.#telemetry.sendMetric('wasmDownloadTime', time);

            return buffer;
        } catch {
            return new LoadError();
        }
    }

    async #loadModelsData(): Promise<ArrayBuffer | LoadError> {
        try {
            const [buffer, time] = await downloadFileAsBuffer(new URL('../bin/Spench.data', import.meta.url));
            this.#telemetry.sendMetric('modelsDataDownloadTime', time);

            return buffer;
        } catch {
            return new LoadError();
        }
    }

    #createWorker(): Worker {
        // Don't allow the bundler to process prepared worker
        const url = new URL('../dist/workers/spench-worker.min.js', import.meta.url);

        return new Worker(url);
    }
}

export default SpenchSDK;
