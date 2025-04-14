import SpenchError from './utils/error';
import { SpenchAudioNode, SpenchAudioNodeImpl } from './audioNode';
import { downloadFileAsBuffer, Features, SharedFeatures } from './utils/common';
import createLogger, { ExternalLogger, Logger } from './utils/logger';
import { createAudioNodeTelemetry, LoaderTelemetry, SendMetricFunction } from './utils/telemetry';
import SpenchWorker from './worker';

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

    #preloadedWorker: SpenchWorker | LoadError | undefined;

    #sharedWorker: SpenchWorker | undefined;

    #workletLoadPromise: Promise<boolean | LoadError> | undefined;

    #isWorkletRegistered = false;

    protected constructor(
        ctx: AudioContext,
        logger: Logger,
        sendMetric: SendMetricFunction | undefined,
        private isDebug: boolean,
    ) {
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
        const instance = new SpenchSDK(ctx, logger, sendMetric, Boolean(isDebug));

        if (preloadFeatures) {
            instance.#preloadFeatures(preloadFeatures);
        }

        return instance;
    }

    async createAudioNode(features: Features, shared?: false): Promise<SpenchAudioNode | CreateAudioNodeError>;

    async createAudioNode(features: SharedFeatures, shared: true): Promise<SpenchAudioNode | CreateAudioNodeError>;

    async createAudioNode(
        features: Features = { normalization: true },
        shared = false,
    ): Promise<SpenchAudioNode | CreateAudioNodeError> {
        const isTelemetryEnabled = Boolean(this.#sendMetricFunction);

        if (shared && this.#sharedWorker) {
            this.#logger.debug('using shared worker');

            return new SpenchAudioNodeImpl({
                features,
                ctx: this.#ctx,
                worker: this.#sharedWorker,
                logger: this.#logger,
                isDebug: this.isDebug,
                telemetry: isTelemetryEnabled
                    ? createAudioNodeTelemetry(this.#logger, this.#sendMetricFunction)
                    : undefined,
            });
        }

        const workletPromise = Promise.resolve(this.#workletLoadPromise).then((res) =>
            res === true ? res : this.#loadSpenchWorklet(),
        );

        const wasmPromise = Promise.resolve(this.#wasmLoadPromise).then((res) =>
            res instanceof ArrayBuffer ? res : this.#loadWasm(),
        );

        const dataPromise = Promise.resolve(this.#dataLoadPromise).then((res) => {
            if (res instanceof ArrayBuffer) {
                return res;
            }

            if (features.noiseCancellation || features.echoCancellation) {
                return this.#loadModelsData();
            }

            // eslint-disable-next-line no-useless-return
            return;
        });

        const worker = this.#preloadedWorker || this.#createWorker();
        const [workletResult, wasm, data] = await Promise.all([workletPromise, wasmPromise, dataPromise]);

        if (
            wasm instanceof LoadError ||
            data instanceof LoadError ||
            worker instanceof LoadError ||
            workletResult !== true
        ) {
            return new CreateAudioNodeError();
        }

        worker.init(this.isDebug, Boolean(this.#sendMetricFunction), wasm, data);

        if (shared) {
            worker.weak = false;
            this.#sharedWorker = worker;
        }

        // Can't reuse ArrayBuffer as it's transferred to the different context
        // Also binaries are quite big, so we rely on the browser cache
        this.#wasmLoadPromise = undefined;
        this.#dataLoadPromise = undefined;
        this.#preloadedWorker = undefined;

        return new SpenchAudioNodeImpl({
            features,
            ctx: this.#ctx,
            worker,
            logger: this.#logger,
            isDebug: this.isDebug,
            telemetry: isTelemetryEnabled
                ? createAudioNodeTelemetry(this.#logger, this.#sendMetricFunction)
                : undefined,
        });
    }

    // TODO link rel prefetch / url list for preloading / maybe load by consumer app
    #preloadFeatures(features: Features): void {
        this.#wasmLoadPromise = this.#loadWasm();

        if (features.echoCancellation || features.noiseCancellation) {
            this.#dataLoadPromise = this.#loadModelsData();
        }
        this.#preloadedWorker = this.#createWorker();
        this.#workletLoadPromise = this.#loadSpenchWorklet();
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

    async #loadWasm(): Promise<ArrayBuffer | LoadError> {
        try {
            const [buffer, time] = await downloadFileAsBuffer(new URL('../bin/spench.wasm', import.meta.url));
            this.#telemetry.sendMetric('wasmDownloadTime', time);

            return buffer;
        } catch (e) {
            return new LoadError(e);
        }
    }

    async #loadModelsData(): Promise<ArrayBuffer | LoadError> {
        try {
            const [buffer, time] = await downloadFileAsBuffer(new URL('../bin/spench.data', import.meta.url));
            this.#telemetry.sendMetric('modelsDataDownloadTime', time);

            return buffer;
        } catch (e) {
            return new LoadError(e);
        }
    }

    #createWorker(): SpenchWorker | LoadError {
        try {
            return new SpenchWorker(this.#logger);
        } catch (e) {
            return new LoadError(e);
        }
    }
}

export default SpenchSDK;
