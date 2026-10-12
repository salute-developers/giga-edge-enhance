import { deserializeError } from 'serialize-error';

import { Logger } from './utils/logger';
import { Telemetry } from './utils/telemetry';
import GigaEdgeSpenchError, { assert } from './utils/error';
import {
    CONTROLLER_MESSAGES,
    ControllerMessage,
    CustomPort,
    WORKER_MESSAGES,
    WorkerMessage,
    WORKLET_MESSAGES,
    WorkletMessage,
} from './utils/ipc';
import {
    areMainFeaturesEnabled,
    areDspFeaturesEnabled,
    Features,
    getDspFeatures,
    Settings,
    DspFeatures,
    areDspFeaturesChanged,
    areMainFeaturesChanged,
    ModelsTmpl,
    getDefaultNoiseCancellationModel,
    getMainFeatures,
    isSharedMemorySupported,
} from './utils/common';
import {
    getBufferSize,
    getInputBufferSeconds,
    getOutputBufferSeconds,
    LP_CAUSE_THRESHOLD,
    GES_DSP_WORKLET_NAME,
    GES_WORKLET_NAME,
} from './utils/constants';
import GigaEdgeSpenchWorker from './worker';
import Loader, { LoadError } from './loader';
import Deferred from './utils/deferred';
import { RingBuffer } from './utils/ringBuffer';
import { BinarySemaphore } from './utils/semaphore';

/**
 * Represents an error that occurs when changing feature configuration fails in the
 * {@link GigaEdgeSpenchAudioNode}. This error is a thin wrapper around {@link GigaEdgeSpenchError}
 * that can be distinguished in the error handling logic.
 */
export class SetFeaturesFailedError extends GigaEdgeSpenchError {
    override get name() {
        return 'SetFeaturesFailedError';
    }
}

/**
 * Possible causes for a low‑performance event emitted by the audio node.
 *
 * * `transport` – latency introduced by the worker/message channel.
 * * `inference` – heavy inference processing on the DSP thread.
 * * `unknown` – the cause could not be determined.
 */
type LowPerformanceCause = 'transport' | 'inference' | 'unknown';

/**
 * Custom event that is dispatched when the audio node is unable to operate
 * in real‑time mode under the current environmental conditions. The event
 * carries the most probable cause and the mean process time observed.
 *
 * @extends Event
 */
class LowPerformanceEvent extends Event {
    /**
     * @param {LowPerformanceCause} potentialCause - The most likely cause.
     * @param {number|undefined} [meanProcessTime] - The mean processing time in milliseconds.
     */
    constructor(
        public potentialCause: LowPerformanceCause,
        public meanProcessTime?: number,
    ) {
        super('low_performance', { cancelable: true });
    }
}

/**
 * Map of event names to their corresponding event objects for
 * {@link GigaEdgeSpenchAudioNode}. Used for type‑safe event handling.
 */
interface GigaEdgeSpenchAudioNodeEventMap {
    low_performance: LowPerformanceEvent;
    destroyed: Event;
}

/**
 * A port that the worklet node uses to communicate with the processor
 * (`CustomPort` typed for controller ↔ worklet messages).
 */
type WorkletPort = CustomPort<ControllerMessage, WorkletMessage>;

/**
 * A port that the worklet node uses to communicate with the Web Worker
 * (`CustomPort` typed for controller ↔ worker messages).
 */
type WorkerPort = CustomPort<ControllerMessage, WorkerMessage>;

/**
 * Public API surface of the GigaEdgeSpench audio node.  Extends the native
 * {@link AudioWorkletNode} with GigaEdgeSpench‑specific control methods and
 * custom events.
 *
 * @template M - Template for the available GigaEdgeSpench models.
 */
export interface GigaEdgeSpenchAudioNode<M extends ModelsTmpl> extends AudioWorkletNode {
    /**
     * Configures the node with the specified features.
     *
     * @param {Partial<Features<M>>} features - Features to enable or disable.
     * @returns {Promise<void | LoadError | SetFeaturesFailedError>} A promise that
     * resolves when the configuration has finished or resolves with an error.
     */
    setFeatures(features: Partial<Features<M>>): Promise<void | LoadError | SetFeaturesFailedError>;

    /** Immediately resets the internal state of the node and its worker. */
    reset(): void;

    /** Cleans up all resources of the node. */
    destroy(): void;

    /**
     * Adds an event listener for events emitted by this node.
     *
     * @template K - Key of the event map.
     * @param {K} type - Name of the event.
     * @param {(this: GigaEdgeSpenchAudioNode<M>, ev: GigaEdgeSpenchAudioNodeEventMap[K]) => any} listener - Callback.
     * @param {boolean|AddEventListenerOptions} [options] - Listener options.
     */
    addEventListener<K extends keyof GigaEdgeSpenchAudioNodeEventMap>(
        type: K,
        listener: (this: GigaEdgeSpenchAudioNode<M>, ev: GigaEdgeSpenchAudioNodeEventMap[K]) => any,
        options?: boolean | AddEventListenerOptions,
    ): void;

    /**
     * Removes a previously added event listener.
     *
     * @template K - Key of the event map.
     * @param {K} type - Name of the event.
     * @param {(this: GigaEdgeSpenchAudioNode<M>, ev: GigaEdgeSpenchAudioNodeEventMap[K]) => any} listener - Callback.
     * @param {boolean|EventListenerOptions} [options] - Listener options.
     */
    removeEventListener<K extends keyof GigaEdgeSpenchAudioNodeEventMap>(
        type: K,
        listener: (this: GigaEdgeSpenchAudioNode<M>, ev: GigaEdgeSpenchAudioNodeEventMap[K]) => any,
        options?: boolean | EventListenerOptions,
    ): void;
}

/**
 * Parameters required to construct a {@link GigaEdgeSpenchAudioNodeImpl}.
 *
 * @template M - Template for the available GigaEdgeSpench models.
 */
interface GigaEdgeSpenchAudioNodeCtorParams<M extends ModelsTmpl> {
    logger: Logger;
    ctx: AudioContext;
    loader: Loader<M>;
    telemetry: Telemetry;
    settings: Settings;
}

/**
 * Implementation of the GigaEdgeSpench audio node.  It orchestrates communication
 * with the AudioWorkletProcessor, a Web Worker, and the optional
 * DSP‑WASM module.  The class handles feature loading, dynamic re‑configuration,
 * error reporting, and state synchronization with the audio context.
 *
 * @template M - Template for the available GigaEdgeSpench models.
 */
export class GigaEdgeSpenchAudioNodeImpl<M extends ModelsTmpl> extends AudioWorkletNode implements GigaEdgeSpenchAudioNode<M> {
    /** Logger instance scoped to this node. */
    readonly #logger: Logger;

    /** Resource loader for models, WASM, and worker. */
    readonly #loader: Loader<M>;

    /** Current user settings. */
    readonly #settings: Settings;

    /** Telemetry client used for metrics. */
    readonly #telemetry: Telemetry;

    /** Current feature configuration. */
    #features: Features<M> = {
        normalization: false,
        echoCancellation: false,
        noiseCancellation: false,
    };

    /** Reference to the worker thread, if running. */
    #worker: GigaEdgeSpenchWorker | undefined;

    /** Message channel port used to talk to the worker. */
    #workerPort: WorkerPort | undefined;

    /** Indicates whether the node has finished its first processing cycle. */
    #initialized = false;

    /** Indicates whether the DSP‑WASM module has been loaded. */
    #isDspLoaded = false;

    /** Promise that blocks concurrent calls to {@link setFeatures}. */
    #setFeaturesLock: Promise<void> | undefined;

    #destroyed = false;

    /** Timestamp of the last feature configuration start. */
    #initStartTime = performance.now();

    /**
     * Creates a new instance.
     *
     * @param {GigaEdgeSpenchAudioNodeCtorParams<M>} params - Constructor parameters.
     */
    constructor({ ctx, logger, telemetry, loader, settings }: GigaEdgeSpenchAudioNodeCtorParams<M>) {
        super(ctx, settings.dspOnAudioThread ? GES_DSP_WORKLET_NAME : GES_WORKLET_NAME, {
            numberOfInputs: 2,
            numberOfOutputs: 1,
            outputChannelCount: [1],
        });

        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#loader = loader;
        this.#settings = settings;

        this.#init();
    }

    /** Internal alias for the worklet port. */
    get #port(): WorkletPort {
        return this.port;
    }

    get #useSharedMemory(): boolean {
        return this.#settings.forceSharedMemory || (this.#settings.sharedMemoryEnabled && isSharedMemorySupported());
    }

    /**
     * Configures the audio node with the requested feature set.
     *
     * This method enables or disables the features specified in the `features`
     * object, automatically loads any required models or worker code, and
     * synchronises the configuration across the worker, the worklet, and the
     * node itself.  It returns a promise that resolves once the configuration
     * has finished. Features which are not provided in the input object will be
     * disabled. Returns a promise that resolves to:
     *   • undefined on success
     *   • LoadError if loading any resources failed
     *   • SetFeaturesFailedError if the features could not be applied
     *
     * @param features - A partial set of {@link Features} to enable or disable.
     * @returns {Promise<void | LoadError | SetFeaturesFailedError>}
     */
    async setFeatures(features: Partial<Features<M>>): Promise<void | LoadError | SetFeaturesFailedError> {
        if (this.#setFeaturesLock) {
            return this.#setFeaturesLock.then(() => this.setFeatures(features));
        }
        const { promise, resolve } = new Deferred<void>();
        this.#setFeaturesLock = promise;
        this.#initStartTime = performance.now();
        try {
            const enrichedFeatures: Features<M> = {
                normalization: Boolean(features.normalization),
                echoCancellation: Boolean(features.echoCancellation),
                noiseCancellation:
                    features.noiseCancellation === true
                        ? (getDefaultNoiseCancellationModel(
                              this.#loader.models,
                              this.context.sampleRate,
                          ) as keyof M['noiseCancellation'])
                        : features.noiseCancellation || false,
            };
            this.#logger.info('Setting GigaEdgeSpench features:', enrichedFeatures);

            let res;

            if (this.#settings.dspOnAudioThread) {
                if (areMainFeaturesEnabled(enrichedFeatures)) {
                    this.#logger.warn('Main features are not available when dspOnAudioThread is enabled');
                }

                res = await this.#setDspWorkletFeatures(enrichedFeatures);
            } else if (
                areMainFeaturesEnabled(enrichedFeatures) &&
                areDspFeaturesEnabled(enrichedFeatures) &&
                !this.#workerPort
            ) {
                this.#logger.debug('Starting DSP features first to speed up loading');
                this.#loader.preload(enrichedFeatures);
                const workletFeatures = getDspFeatures(enrichedFeatures);

                res = await this.#setFeatures({ ...workletFeatures, ...getMainFeatures() });

                if (!(res instanceof GigaEdgeSpenchError)) {
                    res = await this.#setFeatures(enrichedFeatures);
                }
            } else {
                res = await this.#setFeatures(enrichedFeatures);
            }

            return res;
        } finally {
            this.#setFeaturesLock = undefined;
            resolve();
        }
    }

    /**
     * Cleans up all resources, detaching event listeners, closing ports and
     * workers, and dispatching a `destroyed` event.
     */
    destroy(): void {
        if (this.#destroyed) return;
        this.#destroyed = true;
        this.#logger.debug('Destroying GigaEdgeSpench AudioNode');

        this.#workerPort?.removeEventListener('message', this.#handleWorkerMessage);
        this.#workerPort?.close();
        this.#workerPort = undefined;
        this.#worker?.removeEventListener('error', this.#handleWorkerError);
        this.#worker = undefined;

        this.#port.postMessage({ type: WORKLET_MESSAGES.DESTROY });
        this.#port.removeEventListener('message', this.#handleWorkletMessage);
        this.#port.removeEventListener('messageerror', this.#handleMessageError);
        this.#port.close();

        super.removeEventListener('processorerror', this.#handleProcessorError);
        this.context.removeEventListener('statechange', this.#handleContextStateChange);

        super.dispatchEvent(new Event('destroyed'));
    }

    /** Resets the internal state of the node and forwards the reset to the worker. */
    reset() {
        this.#logger.debug('Resetting GigaEdgeSpench AudioNode');

        this.#port.postMessage({ type: WORKLET_MESSAGES.RESET });
        this.#workerPort?.postMessage({ type: WORKER_MESSAGES.RESET });
    }

    /**
     * Internal helper that applies a full feature set to the worker (if needed)
     * and forwards the feature list to the worklet processor.
     *
     * @private
     * @param {Features<M>} features - The full feature set to apply.
     * @returns {Promise<void | LoadError | SetFeaturesFailedError>}
     */
    async #setFeatures(features: Features<M>): Promise<void | LoadError | SetFeaturesFailedError> {
        const workerFeaturesEnabled = areMainFeaturesEnabled(features) || areDspFeaturesEnabled(features);
        const workerSetResult = await this.#setWorkerFeatures(features);

        if (workerSetResult instanceof GigaEdgeSpenchError) {
            return workerSetResult;
        }

        this.#port.postMessage({
            type: WORKLET_MESSAGES.SET_FEATURES,
            workerFeaturesEnabled,
        });

        if (!this.#initialized && this.#features.noiseCancellation) {
            this.#initialized = true;
            this.#telemetry?.sendMetric('initTime', performance.now() - this.#initStartTime);
        }
    }

    /**
     * Internal helper that loads the DSP WASM module (if not already loaded)
     * and sends the required feature set to the worklet processor.
     *
     * @private
     * @param {DspFeatures} features - DSP‑specific features to enable.
     * @returns {Promise<void | LoadError | SetFeaturesFailedError>}
     */
    async #setDspWorkletFeatures(features: DspFeatures): Promise<void | LoadError | SetFeaturesFailedError> {
        if (!areDspFeaturesChanged(this.#features, features)) {
            return;
        }

        let dspWasm;

        if (!this.#isDspLoaded) {
            dspWasm = await this.#loader.loadDSPWasm();

            if (dspWasm instanceof LoadError) {
                return dspWasm;
            }

            this.#isDspLoaded = true;
        }

        const dspFeatures = getDspFeatures(features);
        this.#logger.debug('Applying DSP Worklet features:', dspFeatures);
        this.#port.postMessage({
            type: WORKLET_MESSAGES.SET_FEATURES,
            features: dspFeatures,
            dspWasm,
            workerFeaturesEnabled: false,
        });

        return new Promise<void | SetFeaturesFailedError>((resolve) => {
            const cleanup = () => {
                this.#port.removeEventListener('message', handler);
                this.removeEventListener('destroyed', handleDestroyed);
            };
            const finish = (result?: SetFeaturesFailedError) => {
                cleanup();
                resolve(result);
            };
            const handler = (ev: MessageEvent<ControllerMessage>) => {
                if (ev.data.type !== CONTROLLER_MESSAGES.SET_FEATURES_RESULT) {
                    return;
                }

                const {
                    errors: [error],
                    features: resultFeatures,
                } = ev.data;

                this.#features = {
                    ...this.#features,
                    ...resultFeatures,
                };

                if (error) {
                    this.#logger.error('DSP worklet features not applied', error);
                    finish(new SetFeaturesFailedError(error));
                } else {
                    this.#logger.debug('DSP worklet features applied');
                    finish();
                }
            };
            const handleDestroyed = () => finish(new SetFeaturesFailedError('Destroyed while loading'));

            this.#port.addEventListener('message', handler);
            this.addEventListener('destroyed', handleDestroyed, { once: true });
        });
    }

    /**
     * Ensures the worker is running and sends a feature set request to it.
     *
     * @private
     * @param {Features<M>} features - Features to set in the worker.
     * @returns {Promise<void | LoadError | SetFeaturesFailedError>}
     */
    async #setWorkerFeatures(features: Features<M>): Promise<void | LoadError | SetFeaturesFailedError> {
        const areFeaturesChanged =
            areMainFeaturesChanged(this.#features, features) || areDspFeaturesChanged(this.#features, features);

        if (!areFeaturesChanged) {
            return;
        }

        const workerInitResult = await this.#loadWorker();

        if (workerInitResult instanceof LoadError) {
            return workerInitResult;
        }

        this.#logger.debug('Applying features:', features);
        this.#workerPort!.postMessage({
            type: WORKER_MESSAGES.SET_FEATURES,
            features: features as Features<ModelsTmpl>,
        });

        return new Promise<void | LoadError | SetFeaturesFailedError>((resolve) => {
            const workerPort = this.#workerPort!;
            const cleanup = () => {
                workerPort.removeEventListener('message', handler);
                this.removeEventListener('destroyed', handleDestroyed);
            };
            const finish = (result?: LoadError | SetFeaturesFailedError) => {
                cleanup();
                resolve(result);
            };
            const handler = (ev: MessageEvent<ControllerMessage>) => {
                if (ev.data.type !== CONTROLLER_MESSAGES.SET_FEATURES_RESULT) {
                    return;
                }

                const { errors: rawErrors, features: resultFeatures } = ev.data;
                const errors = rawErrors.map((err) => deserializeError(err));

                this.#features = {
                    ...this.#features,
                    ...resultFeatures,
                };

                if (errors.length) {
                    this.#logger.error(
                        'Errors while setting features: ',
                        errors.join('\n'),
                        '\n\nApplied features: ',
                        resultFeatures,
                    );
                }

                const loadError = errors.find((e) => e.name === 'LoadError');
                const criticalError = errors.find((e) => e.name !== 'LoadError');

                if (criticalError) {
                    finish(new SetFeaturesFailedError(criticalError));
                } else if (loadError) {
                    finish(new LoadError(loadError));
                } else {
                    this.#logger.debug('Features applied', resultFeatures);
                    finish();
                }
            };
            const handleDestroyed = () => finish(new LoadError('Destroyed while loading'));

            workerPort.addEventListener('message', handler);
            this.addEventListener('destroyed', handleDestroyed, { once: true });
        });
    }

    /**
     * Checks if a received message is a common message that can be ignored
     * by the node.
     *
     * @private
     * @param {MessageEvent<ControllerMessage>} event - Incoming message.
     * @returns {boolean} `true` if the message is handled.
     */
    #handleCommonMessage(event: MessageEvent<ControllerMessage>, origin: 'Worker' | 'AudioWorkletProcessor'): boolean {
        switch (event.data.type) {
            case CONTROLLER_MESSAGES.SET_FEATURES_RESULT:
                return true;
            case CONTROLLER_MESSAGES.ERROR:
                const { error } = event.data;
                this.#handleError(new GigaEdgeSpenchError(deserializeError(error)), origin);

                return true;
            default:
        }

        return false;
    }

    /**
     * Loads the worker process, establishes the message channels and
     * initializes the worker with the audio context's sample rate.
     *
     * @private
     * @returns {Promise<void | LoadError>}
     */
    async #loadWorker(): Promise<void | LoadError> {
        if (this.#workerPort) return;

        this.#initStartTime = performance.now();

        const { sampleRate } = this.context;
        const inputBufferSeconds = getInputBufferSeconds(this.#settings);
        const outputBufferSeconds = getOutputBufferSeconds(this.#settings);
        const worker = await this.#loader.loadWorker(this.#settings);
        this.#loader.preloadedWorker = undefined;

        if (worker instanceof LoadError) {
            return worker;
        }
        this.#worker = worker;
        this.#workerPort = this.#worker.createPort();
        const workerWorkletChannel = new MessageChannel();
        let buffer: SharedArrayBuffer | undefined;

        if (this.#useSharedMemory) {
            this.#logger.debug('Using SharedArrayBuffer transport');
            buffer = new SharedArrayBuffer(
                2 * RingBuffer.getInternalBufferSize(getBufferSize(sampleRate, inputBufferSeconds)) +
                    RingBuffer.getInternalBufferSize(getBufferSize(sampleRate, outputBufferSeconds)) +
                    BinarySemaphore.INTERNAL_BUFFER_SIZE,
            );
        } else {
            this.#logger.debug('Using MessageChannel transport');
        }

        this.#workerPort.postMessage(
            {
                type: WORKER_MESSAGES.INIT,
                buffer,
                workletPort: workerWorkletChannel.port1,
                sampleRate,
            },
            [workerWorkletChannel.port1],
        );
        this.#port.postMessage(
            {
                type: WORKLET_MESSAGES.WORKER_CREATED,
                workerPort: workerWorkletChannel.port2,
                buffer,
            },
            [workerWorkletChannel.port2],
        );

        this.#workerPort.addEventListener('message', this.#handleWorkerMessage);
        this.#workerPort.start();
        this.#worker.addEventListener('error', this.#handleWorkerError);
    }

    /**
     * Handles messages from the AudioWorkletProcessor.
     *
     * @private
     * @param {MessageEvent<ControllerMessage>} event - Incoming message.
     */
    #handleWorkletMessage: Exclude<WorkletPort['onmessage'], null> = (event): void => {
        if (this.#handleCommonMessage(event, 'AudioWorkletProcessor')) {
            return;
        }

        switch (event.data.type) {
            case CONTROLLER_MESSAGES.LOG:
                this.#logger[event.data.level](...event.data.data);

                return;
            case CONTROLLER_MESSAGES.TELEMETRY:
                this.#telemetry?.sendMetric(event.data.metricName, event.data.value);

                return;
            case CONTROLLER_MESSAGES.LOW_PERFORMANCE:
                let potentialCause: LowPerformanceCause = 'inference';

                if (!this.#useSharedMemory) {
                    potentialCause =
                        this.#telemetry.currentMeanProcessTime === undefined
                            ? 'unknown'
                            : this.#telemetry.currentMeanProcessTime > LP_CAUSE_THRESHOLD
                              ? 'inference'
                              : 'transport';
                }

                if (
                    !super.dispatchEvent(
                        new LowPerformanceEvent(potentialCause, this.#telemetry.currentMeanProcessTime),
                    )
                ) {
                    this.#handleLowPerformance();
                }

                return;
            default:
        }

        assert(false, `Unknown message type from AudioWorkletProcessor: ${event.data.type}`);
    };

    /**
     * Handles messages from the worker. Currently all messages are forwarded
     * to the common handler; this function is a placeholder for future
     * worker‑specific handling.
     *
     * @private
     * @param {MessageEvent<ControllerMessage>} event - Incoming message.
     */
    #handleWorkerMessage: Exclude<WorkerPort['onmessage'], null> = (event): void => {
        if (this.#handleCommonMessage(event, 'Worker')) {
            return;
        }

        assert(false, `Unknown message type from Worker: ${event.data.type}`);
    };

    /** Placeholder for low‑performance handling logic. */
    #handleLowPerformance() {
        // TODO HANDLE DEFAULT LOW PERFORMANCE
    }

    /** Central error handler that logs and destroys the node. */
    #handleError = (error: GigaEdgeSpenchError, origin: 'Worker' | 'AudioWorkletProcessor') => {
        this.#logger.error(`${error} at GigaEdgeSpenchAudioNode (${origin})`);

        this.destroy();
    };

    /** Handles processor‑level errors. */
    #handleProcessorError = () =>
        this.#handleError(new GigaEdgeSpenchError('Error in AudioWorkletProcessor'), 'AudioWorkletProcessor');

    /** Handles worker errors. */
    #handleWorkerError = (event: ErrorEvent) =>
        this.#handleError(new GigaEdgeSpenchError(event.error || event.message), 'Worker');

    /** Handles message‑serialization errors. */
    #handleMessageError = () =>
        this.#handleError(new GigaEdgeSpenchError('Error serializing AudioWorkletProcessor message'), 'AudioWorkletProcessor');

    /** Responds to changes in the audio context state. */
    #handleContextStateChange = () => {
        switch (this.context.state) {
            case 'closed':
                this.destroy();
                break;
            case 'suspended':
                this.reset();
                break;
            default:
        }
    };

    /**
     * Performs initial setup: registers context listeners, attaches worklet ports,
     * and sends the first init message to the processor.
     *
     * @private
     */
    #init(): void {
        this.context.addEventListener('statechange', this.#handleContextStateChange);
        super.addEventListener('processorerror', this.#handleProcessorError);

        this.#port.addEventListener('message', this.#handleWorkletMessage);
        this.#port.addEventListener('messageerror', this.#handleMessageError);
        this.#port.start();

        this.#port.postMessage({
            type: WORKLET_MESSAGES.INIT,
            verbosity: this.#logger.verbosity,
            isTelemetryEnabled: this.#telemetry.enabled,
            settings: this.#settings,
        });
    }
}
