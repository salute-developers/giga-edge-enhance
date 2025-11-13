import SpenchError from './utils/error';
import { SetFeaturesFailedError, SpenchAudioNode, SpenchAudioNodeImpl } from './audioNode';
import { Features, getSettingsWithDefaults, ModelsTmpl, Settings } from './utils/common';
import createLogger, { ExternalLogger, Logger, Verbosity } from './utils/logger';
import { createTelemetry, Telemetry, SendMetricFunction } from './utils/telemetry';
import { LIB_VERSION } from './utils/version';
import Loader, { LoadError } from './loader';
import { DEFAULT_MODELS } from './utils/resources';

type CreateSpenchParams<M extends ModelsTmpl> = {
    logger?: ExternalLogger;
    sendMetric?: SendMetricFunction;
    preloadFeatures?: Partial<Features<M>>;
    loggerVerbosity?: Verbosity;
    models?: M;
    settings?: Partial<Settings>;
};

class SpenchSDK<M extends ModelsTmpl = typeof DEFAULT_MODELS> {
    #ctx: AudioContext;

    #logger: Logger;

    #telemetry: Telemetry;

    #loader: Loader<M>;

    #settings: Settings;

    protected constructor(ctx: AudioContext, logger: Logger, telemetry: Telemetry, settings: Settings, models?: M) {
        this.#ctx = ctx;
        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#settings = settings;
        this.#loader = new Loader(ctx, logger, telemetry, settings, models);
    }

    static isEnvironmentSupported(): boolean {
        // TODO browser versions
        return true;
    }

    // eslint-disable-next-line no-shadow
    static create<M extends ModelsTmpl = typeof DEFAULT_MODELS>(
        ctx: AudioContext,
        {
            loggerVerbosity = 'normal',
            logger: externalLogger,
            preloadFeatures,
            sendMetric,
            models = DEFAULT_MODELS as any,
            settings: rawSettings,
        }: CreateSpenchParams<M> = {},
    ): SpenchSDK<M> {
        const logger = createLogger(loggerVerbosity, externalLogger);
        const telemetry = createTelemetry(sendMetric);
        const settings = getSettingsWithDefaults(rawSettings);
        const instance = new SpenchSDK(ctx, logger, telemetry, settings, models);

        if (preloadFeatures) {
            instance.#loader.preload(preloadFeatures, settings);
        }

        logger.info(
            `SpenchSDK v${LIB_VERSION} created.`,
            `Logger verbosity: ${loggerVerbosity}, ${!externalLogger ? 'not ' : ''}connected with external logger.`,
            Object.keys(preloadFeatures || {}).length > 0
                ? `Preloading features: ${JSON.stringify(preloadFeatures)}.`
                : 'Not preloading features.',
            sendMetric ? 'Piping metrics to provided sendMetric function.' : '',
            `AudioContext sample rate: ${ctx.sampleRate} Hz, state: ${ctx.state}.`,
        );

        return instance;
    }

    async createAudioNode(
        features?: Partial<Features<M>>,
        rawSettings?: Partial<Settings>,
    ): Promise<SpenchAudioNode<M> | LoadError | SetFeaturesFailedError> {
        const settings = getSettingsWithDefaults({ ...this.#settings, ...(rawSettings || {}) });
        this.#loader.preload(features || {}, settings);

        const workletLoad = await this.#loader.loadSpenchWorklet(settings);

        if (workletLoad instanceof LoadError) {
            return workletLoad;
        }

        const node = new SpenchAudioNodeImpl<M>({
            loader: this.#loader,
            ctx: this.#ctx,
            logger: this.#logger,
            telemetry: this.#telemetry,
            settings,
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
