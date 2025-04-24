import SpenchError from './utils/error';
import { SpenchAudioNode, SpenchAudioNodeImpl } from './audioNode';
import { Features } from './utils/common';
import createLogger, { ExternalLogger, Logger, Verbosity } from './utils/logger';
import { createTelemetry, Telemetry, SendMetricFunction } from './utils/telemetry';
import Loader, { LoadError } from './loader';

class CreateAudioNodeError extends SpenchError {}

type CreateSpenchParams = {
    logger?: ExternalLogger;
    sendMetric?: SendMetricFunction;
    preloadFeatures?: Features;
    loggerVerbosity?: Verbosity;
};

class SpenchSDK {
    #ctx: AudioContext;

    #logger: Logger;

    #telemetry: Telemetry;

    #loader: Loader;

    protected constructor(ctx: AudioContext, logger: Logger, telemetry: Telemetry) {
        this.#ctx = ctx;
        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#loader = new Loader(ctx, logger, this.#telemetry);
    }

    static isEnvironmentSupported(): boolean {
        // TODO browser versions
        return true;
    }

    static create(
        ctx: AudioContext,
        { loggerVerbosity = 'normal', logger: externalLogger, preloadFeatures, sendMetric }: CreateSpenchParams = {},
    ): SpenchSDK {
        const logger = createLogger(loggerVerbosity, externalLogger);
        const telemetry = createTelemetry(sendMetric);
        const instance = new SpenchSDK(ctx, logger, telemetry);

        if (preloadFeatures) {
            instance.#loader.preload(preloadFeatures);
        }

        logger.info(
            'SpenchSDK created.',
            `Logger verbosity: ${loggerVerbosity}, ${!externalLogger ? 'not ' : ''}connected with external logger.`,
            Object.keys(preloadFeatures || {}).length > 0
                ? `Preloading features: ${JSON.stringify(preloadFeatures)}.`
                : 'Not preloading features.',
            sendMetric ? 'Piping metrics to provided sendMetric function.' : '',
            `AudioContext sample rate: ${ctx.sampleRate} Hz, state: ${ctx.state}.`,
        );

        return instance;
    }

    async createAudioNode(features: Features = {}): Promise<SpenchAudioNode | CreateAudioNodeError> {
        this.#loader.preload(features);

        const workletLoad = await this.#loader.loadSpenchWorklet();

        if (workletLoad instanceof LoadError) {
            return new CreateAudioNodeError();
        }

        return new SpenchAudioNodeImpl({
            features,
            loader: this.#loader,
            ctx: this.#ctx,
            logger: this.#logger,
            telemetry: this.#telemetry,
        });
    }
}

export default SpenchSDK;
