/* eslint-disable no-shadow */
import SpenchError from './utils/error';
import { SetFeaturesFailedError, SpenchAudioNode, SpenchAudioNodeImpl } from './audioNode';
import { Features, getSettingsWithDefaults, ModelsTmpl, Settings } from './utils/common';
import createLogger, { ExternalLogger, Logger, Verbosity } from './utils/logger';
import { createTelemetry, Telemetry, SendMetricFunction } from './utils/telemetry';
import { LIB_VERSION } from './utils/version';
import Loader, { LoadError } from './loader';
import { DEFAULT_MODELS, getDefaultModels } from './utils/resources';

/**
 * Configuration options used to create a {@link SpenchSDK} instance.
 *
 * @template M - The model template used by the SDK.
 * @property {ExternalLogger} [logger] - External logger to use.
 * @property {SendMetricFunction} [sendMetric] - Function to send telemetry metrics.
 * @property {Partial<Features<M>>} [preloadFeatures] - Features to preload before use.
 * @property {Verbosity} [loggerVerbosity] - Verbosity level for the internal logger.
 * @property {Partial<M>} [models] - Custom models to override defaults.
 * @property {Partial<Settings>} [settings] - Custom settings to override defaults.
 */
type CreateSpenchParams<M extends ModelsTmpl> = {
    logger?: ExternalLogger;
    sendMetric?: SendMetricFunction;
    preloadFeatures?: Partial<Features<M>>;
    loggerVerbosity?: Verbosity;
    models?: Partial<M>;
    settings?: Partial<Settings>;
};

/**
 * Represents the main SDK class for interacting with Spench features.
 *
 * @template M - The model template for the SDK, defaulting to {@link DEFAULT_MODELS}.
 */
class SpenchSDK<M extends ModelsTmpl = typeof DEFAULT_MODELS> {
    #ctx: AudioContext;

    #logger: Logger;

    #telemetry: Telemetry;

    #loader: Loader<M>;

    #settings: Settings;

    /**
     * Creates a new instance of {@link SpenchSDK}.
     *
     * @param ctx - The {@link AudioContext} to use for the SDK.
     * @param logger - The external logger instance.
     * @param telemetry - The telemetry instance for metric collection.
     * @param settings - The settings for the SDK.
     * @param models - Optional custom models to use.
     */
    protected constructor(ctx: AudioContext, logger: Logger, telemetry: Telemetry, settings: Settings, models?: M) {
        this.#ctx = ctx;
        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#settings = settings;
        this.#loader = new Loader(ctx, logger, telemetry, settings, models);
    }

    /**
     * Checks if the current environment supports the required Web Audio features.
     *
     * @returns {boolean} `true` if supported, otherwise `false`.
     */
    static isEnvironmentSupported(): boolean {
        if (typeof window === 'undefined' || typeof Worker !== 'function' || typeof AudioWorkletNode !== 'function') {
            return false;
        }

        const AudioContextConstructor = window.AudioContext || (window as any).webkitAudioContext;

        return typeof AudioContextConstructor === 'function' && 'audioWorklet' in AudioContextConstructor.prototype;
    }

    /**
     * Factory method to create a new {@link SpenchSDK} instance.
     *
     * @param ctx - The {@link AudioContext} to use.
     * @param params - Optional configuration parameters {@link CreateSpenchParams}.
     * @returns A new {@link SpenchSDK} instance.
     */
    static create<M extends ModelsTmpl = typeof DEFAULT_MODELS>(
        ctx: AudioContext,
        {
            loggerVerbosity = 'normal',
            logger: externalLogger,
            preloadFeatures,
            sendMetric,
            models: rawModels = DEFAULT_MODELS as any,
            settings: rawSettings,
        }: CreateSpenchParams<M> = {},
    ): SpenchSDK<typeof DEFAULT_MODELS & M> {
        const logger = createLogger(loggerVerbosity, externalLogger);
        const models = getDefaultModels(rawModels);
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

    /**
     * Creates an {@link SpenchAudioNode} with optional features and settings.
     *
     * @param features - Features to enable on the node.
     * @param rawSettings - Optional additional settings to merge with the SDK defaults.
     * @returns A promise that resolves to an {@link SpenchAudioNode} or an error type.
     */
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
