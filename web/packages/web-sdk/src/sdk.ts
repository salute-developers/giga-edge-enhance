/* eslint-disable no-shadow */
import GigaEdgeEnhanceError from './utils/error';
import { SetFeaturesFailedError, GigaEdgeEnhanceAudioNode, GigaEdgeEnhanceAudioNodeImpl } from './audioNode';
import { Features, getSettingsWithDefaults, ModelsTmpl, Settings } from './utils/common';
import createLogger, { ExternalLogger, Logger, Verbosity } from './utils/logger';
import { createTelemetry, Telemetry, SendMetricFunction } from './utils/telemetry';
import { LIB_VERSION } from './utils/version';
import Loader, { LoadError } from './loader';
import { DEFAULT_MODELS, getDefaultModels } from './utils/resources';

/**
 * Configuration options used to create a {@link GigaEdgeEnhanceSDK} instance.
 *
 * @template M - The model template used by the SDK.
 * @property {ExternalLogger} [logger] - External logger to use.
 * @property {SendMetricFunction} [sendMetric] - Function to send telemetry metrics.
 * @property {Partial<Features<M>>} [preloadFeatures] - Features to preload before use.
 * @property {Verbosity} [loggerVerbosity] - Verbosity level for the internal logger.
 * @property {Partial<M>} [models] - Custom models to override defaults.
 * @property {Partial<Settings>} [settings] - Custom settings to override defaults.
 */
type CreateGigaEdgeEnhanceParams<M extends ModelsTmpl> = {
    logger?: ExternalLogger;
    sendMetric?: SendMetricFunction;
    preloadFeatures?: Partial<Features<M>>;
    loggerVerbosity?: Verbosity;
    models?: Partial<M>;
    settings?: Partial<Settings>;
};

/**
 * Represents the main SDK class for interacting with GigaEdgeEnhance features.
 *
 * @template M - The model template for the SDK, defaulting to {@link DEFAULT_MODELS}.
 */
class GigaEdgeEnhanceSDK<M extends ModelsTmpl = typeof DEFAULT_MODELS> {
    #ctx: AudioContext;

    #logger: Logger;

    #telemetry: Telemetry;

    #loader: Loader<M>;

    #settings: Settings;

    /**
     * Creates a new instance of {@link GigaEdgeEnhanceSDK}.
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
     * Factory method to create a new {@link GigaEdgeEnhanceSDK} instance.
     *
     * @param ctx - The {@link AudioContext} to use.
     * @param params - Optional configuration parameters {@link CreateGigaEdgeEnhanceParams}.
     * @returns A new {@link GigaEdgeEnhanceSDK} instance.
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
        }: CreateGigaEdgeEnhanceParams<M> = {},
    ): GigaEdgeEnhanceSDK<typeof DEFAULT_MODELS & M> {
        const logger = createLogger(loggerVerbosity, externalLogger);
        const models = getDefaultModels(rawModels);
        const telemetry = createTelemetry(sendMetric);
        const settings = getSettingsWithDefaults(rawSettings);
        const instance = new GigaEdgeEnhanceSDK(ctx, logger, telemetry, settings, models);

        if (preloadFeatures) {
            instance.#loader.preload(preloadFeatures, settings);
        }

        logger.info(
            `GigaEdgeEnhanceSDK v${LIB_VERSION} created.`,
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
     * Creates an {@link GigaEdgeEnhanceAudioNode} with optional features and settings.
     *
     * @param features - Features to enable on the node.
     * @param rawSettings - Optional additional settings to merge with the SDK defaults.
     * @returns A promise that resolves to an {@link GigaEdgeEnhanceAudioNode} or an error type.
     */
    async createAudioNode(
        features?: Partial<Features<M>>,
        rawSettings?: Partial<Settings>,
    ): Promise<GigaEdgeEnhanceAudioNode<M> | LoadError | SetFeaturesFailedError> {
        const settings = getSettingsWithDefaults({ ...this.#settings, ...(rawSettings || {}) });
        this.#loader.preload(features || {}, settings);

        const workletLoad = await this.#loader.loadGigaEdgeEnhanceWorklet(settings);

        if (workletLoad instanceof LoadError) {
            return workletLoad;
        }

        const node = new GigaEdgeEnhanceAudioNodeImpl<M>({
            loader: this.#loader,
            ctx: this.#ctx,
            logger: this.#logger,
            telemetry: this.#telemetry,
            settings,
        });

        if (!features) {
            return node;
        }

        try {
            const res = await node.setFeatures(features);

            if (res instanceof GigaEdgeEnhanceError) {
                node.destroy();

                return res;
            }
        } catch (error) {
            node.destroy();
            throw error;
        }

        return node;
    }
}

export default GigaEdgeEnhanceSDK;
