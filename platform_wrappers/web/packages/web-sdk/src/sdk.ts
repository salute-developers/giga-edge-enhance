import SpenchError from './utils/error';
import { SetFeaturesFailedError, SpenchAudioNode, SpenchAudioNodeImpl } from './audioNode';
import { Features, getSettingsWithDefaults, Settings } from './utils/common';
import createLogger, { ExternalLogger, Logger, Verbosity } from './utils/logger';
import { createTelemetry, Telemetry, SendMetricFunction } from './utils/telemetry';
import Loader, { LoadError } from './loader';

type CreateSpenchParams = {
    logger?: ExternalLogger;
    sendMetric?: SendMetricFunction;
    preloadFeatures?: Features;
    loggerVerbosity?: Verbosity;
    settings?: Partial<Settings>;
};

class SpenchSDK {
    #ctx: AudioContext;

    #logger: Logger;

    #telemetry: Telemetry;

    #loader: Loader;

    #settings: Settings;

    protected constructor(ctx: AudioContext, logger: Logger, telemetry: Telemetry, settings: Settings) {
        this.#ctx = ctx;
        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#settings = settings;
        this.#loader = new Loader(ctx, logger, this.#telemetry);
    }

    static isEnvironmentSupported(): boolean {
        // TODO browser versions
        return true;
    }

    static create(
        ctx: AudioContext,
        {
            loggerVerbosity = 'normal',
            logger: externalLogger,
            preloadFeatures,
            sendMetric,
            settings: rawSettings,
        }: CreateSpenchParams = {},
    ): SpenchSDK {
        const logger = createLogger(loggerVerbosity, externalLogger);
        const telemetry = createTelemetry(sendMetric);
        const settings = getSettingsWithDefaults(rawSettings);
        const instance = new SpenchSDK(ctx, logger, telemetry, settings);

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

    async createAudioNode(features?: Features): Promise<SpenchAudioNode | LoadError | SetFeaturesFailedError> {
        this.#loader.preload(features || {});

        const workletLoad = await this.#loader.loadSpenchWorklet();

        if (workletLoad instanceof LoadError) {
            return workletLoad;
        }

        const node = new SpenchAudioNodeImpl({
            loader: this.#loader,
            ctx: this.#ctx,
            logger: this.#logger,
            telemetry: this.#telemetry,
            settings: this.#settings,
        });

        if (!features) {
            return node;
        }

        const res = await node.setFeatures(features);

        if (res instanceof SpenchError) {
            return res;
        }

        return node;
    }
}

export default SpenchSDK;
