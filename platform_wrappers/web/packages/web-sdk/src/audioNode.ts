import { Logger } from './utils/logger';
import { Telemetry } from './utils/telemetry';
import SpenchError, { assert } from './utils/error';
import {
    CONTROLLER_MESSAGES,
    ControllerMessage,
    CustomPort,
    WORKER_MESSAGES,
    WorkerMessage,
    WORKLET_MESSAGES,
    WorkletMessage,
} from './utils/ipc';
import { areWorkerFeaturesEnabled, areWorkletFeaturesEnabled, Features, Settings } from './utils/common';
import { LP_CAUSE_THRESHOLD, SPENCH_WORKLET_NAME } from './utils/constants';
import SpenchWorker from './worker';
import Loader, { LoadError } from './loader';
import Deferred from './utils/deferred';

export class SetFeaturesFailedError extends SpenchError {}

type LowPerformanceCause = 'transport' | 'inference' | 'unknown';

class LowPerformanceEvent extends Event {
    constructor(
        public potentialCause: LowPerformanceCause,
        public meanProcessTime?: number,
    ) {
        super('low_performance', { cancelable: true });
    }
}

interface SpenchAudioNodeEventMap {
    low_performance: LowPerformanceEvent;
    destroyed: Event;
}

type WorkletPort = CustomPort<ControllerMessage, WorkletMessage>;
type WorkerPort = CustomPort<ControllerMessage, WorkerMessage>;

export interface SpenchAudioNode extends AudioWorkletNode {
    setFeatures(features: Features): Promise<void | LoadError | SetFeaturesFailedError>;
    reset(): void;
    destroy(): void;

    addEventListener<K extends keyof SpenchAudioNodeEventMap>(
        type: K,
        listener: (this: SpenchAudioNode, ev: SpenchAudioNodeEventMap[K]) => any,
        options?: boolean | AddEventListenerOptions,
    ): void;

    removeEventListener<K extends keyof SpenchAudioNodeEventMap>(
        type: K,
        listener: (this: SpenchAudioNode, ev: SpenchAudioNodeEventMap[K]) => any,
        options?: boolean | EventListenerOptions,
    ): void;
}

interface SpenchAudioNodeCtorParams {
    logger: Logger;
    ctx: AudioContext;
    loader: Loader;
    telemetry: Telemetry;
    settings: Settings;
}

export class SpenchAudioNodeImpl extends AudioWorkletNode implements SpenchAudioNode {
    readonly #logger: Logger;

    readonly #loader: Loader;

    readonly #settings: Settings;

    readonly #telemetry: Telemetry;

    #features: Features = {};

    #worker: SpenchWorker | undefined;

    #workerPort: WorkerPort | undefined;

    #initialized = false;

    #isDspLoaded = false;

    #currentMeanProcessTime: number | undefined;

    #setFeaturesLock: Promise<void> | undefined;

    #initStartTime = performance.now();

    constructor({ ctx, logger, telemetry, loader, settings }: SpenchAudioNodeCtorParams) {
        super(ctx, SPENCH_WORKLET_NAME, {
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

    get #port(): WorkletPort {
        return super.port;
    }

    async setFeatures(features: Features): Promise<void | LoadError | SetFeaturesFailedError> {
        if (this.#setFeaturesLock) {
            return this.#setFeaturesLock.then(() => this.setFeatures(features));
        }
        const { promise, resolve } = new Deferred<void>();
        this.#setFeaturesLock = promise;

        const filteredFeatures: Features = Object.fromEntries(
            Object.entries(features).filter(([, value]) => value !== undefined),
        );
        this.#logger.info('Setting Spench features:', filteredFeatures);
        const res = await this.#setFeatures(filteredFeatures);

        this.#setFeaturesLock = undefined;
        resolve();

        return res;
    }

    destroy(): void {
        this.#logger.debug('Destroying Spench AudioNode');

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
        super.context.removeEventListener('statechange', this.#handleContextStateChange);

        super.dispatchEvent(new Event('destroyed'));
    }

    reset() {
        this.#logger.debug('Resetting Spench AudioNode');

        this.#port.postMessage({ type: WORKLET_MESSAGES.RESET });
        this.#workerPort?.postMessage({ type: WORKER_MESSAGES.RESET });
    }

    async #setFeatures(features: Features): Promise<void | LoadError | SetFeaturesFailedError> {
        const loadResult = await Promise.all([this.#loadWorkletFeatures(features), this.#loadWorkerFeatures(features)]);
        const err = loadResult.find((r) => r instanceof SpenchError);
        const previousFeatures = { ...this.#features };

        if (err instanceof SpenchError) {
            return err;
        }

        const workerSetResult = await this.#setWorkerFeatures(features);

        if (workerSetResult instanceof SpenchError) {
            return workerSetResult;
        }

        const workletSetResult = await this.#setWorkletFeatures(features, previousFeatures);

        if (workletSetResult instanceof SpenchError) {
            return workletSetResult;
        }

        if (!this.#initialized && (this.#features.noiseCancellation || this.#features.dereverberation)) {
            this.#initialized = true;
            this.#telemetry?.sendMetric('initTime', performance.now() - this.#initStartTime);
        }
    }

    async #setWorkletFeatures(features: Features, previousFeatures: Features): Promise<void | SetFeaturesFailedError> {
        const workerFeaturesEnabled = areWorkerFeaturesEnabled(features);
        const shouldToggleWorkerFeatures = workerFeaturesEnabled !== areWorkerFeaturesEnabled(previousFeatures);
        const areWorkletFeaturesChanged = [
            typeof features.normalization !== 'undefined' &&
                (this.#features.normalization || false) !== (features.normalization || false),
            typeof features.echoCancellation !== 'undefined' &&
                (this.#features.echoCancellation || false) !== (features.echoCancellation || false),
        ].some(Boolean);
        const mixedFeatures = {
            ...this.#features,
            ...features,
        };
        const { normalization = false, echoCancellation } = mixedFeatures;
        const workletFeatures = {
            normalization,
            echoCancellation,
        };

        if (areWorkletFeaturesChanged || shouldToggleWorkerFeatures) {
            this.#logger.debug('Applying Worklet features:', workletFeatures);
            this.#port.postMessage({
                type: WORKLET_MESSAGES.SET_FEATURES,
                features: workletFeatures,
                workerFeaturesEnabled,
            });

            const res = await new Promise<void | SetFeaturesFailedError>((resolve) => {
                const handler = (ev: MessageEvent<ControllerMessage>) => {
                    switch (ev.data.type) {
                        case CONTROLLER_MESSAGES.SET_FEATURES_OK:
                            this.#logger.debug('Worklet features applied');
                            this.#features = {
                                ...this.#features,
                                ...workletFeatures,
                            };
                            resolve();

                            break;
                        case CONTROLLER_MESSAGES.SET_FEATURES_FAIL:
                            const { error } = ev.data;

                            this.#logger.error('Worklet features not applied', error);
                            resolve(new SetFeaturesFailedError(error));

                            break;
                        default:
                            return;
                    }

                    this.#port.removeEventListener('message', handler);
                };

                this.#port.addEventListener('message', handler);
            });

            if (res instanceof SetFeaturesFailedError) {
                return res;
            }
        }
    }

    async #setWorkerFeatures(features: Features): Promise<void | SetFeaturesFailedError> {
        const areWorkerFeaturesChanged = [
            typeof features.noiseCancellation !== 'undefined' &&
                (this.#features.noiseCancellation || false) !== (features.noiseCancellation || false),
            typeof features.dereverberation !== 'undefined' &&
                (this.#features.dereverberation || false) !== (features.dereverberation || false),
        ].some(Boolean);
        const mixedFeatures = {
            ...this.#features,
            ...features,
        };
        const { noiseCancellation = false, dereverberation = false } = mixedFeatures;
        const workerFeatures = { noiseCancellation, dereverberation };

        if (areWorkerFeaturesChanged) {
            this.#logger.debug('Applying Worker features:', workerFeatures);
            this.#workerPort!.postMessage({ type: WORKER_MESSAGES.SET_FEATURES, features: workerFeatures });

            const res = await new Promise<void | SetFeaturesFailedError>((resolve) => {
                const handler = (ev: MessageEvent<ControllerMessage>) => {
                    switch (ev.data.type) {
                        case CONTROLLER_MESSAGES.SET_FEATURES_OK:
                            this.#logger.debug('Worker features applied');
                            this.#features = {
                                ...this.#features,
                                ...workerFeatures,
                            };
                            resolve();

                            break;
                        case CONTROLLER_MESSAGES.SET_FEATURES_FAIL:
                            const { error } = ev.data;

                            this.#logger.error('Worker features not applied', error);
                            resolve(new SetFeaturesFailedError(error));

                            break;
                        default:
                            return;
                    }

                    this.#workerPort!.removeEventListener('message', handler);
                };

                this.#workerPort!.addEventListener('message', handler);
            });

            if (res instanceof SetFeaturesFailedError) {
                return res;
            }
        }
    }

    #handleCommonMessage(event: MessageEvent<ControllerMessage>): boolean {
        switch (event.data.type) {
            case CONTROLLER_MESSAGES.TELEMETRY:
                if (event.data.metricName === 'currentMeanProcessTime') {
                    this.#currentMeanProcessTime = event.data.value as number;

                    return true;
                }

                this.#telemetry?.sendMetric(event.data.metricName, event.data.value);

                return true;
            case CONTROLLER_MESSAGES.SET_FEATURES_OK:
            case CONTROLLER_MESSAGES.SET_FEATURES_FAIL:
                return true;
            default:
        }

        return false;
    }

    async #loadWorkletFeatures(features: Features): Promise<void | LoadError> {
        if (this.#isDspLoaded) return;

        const workletFeaturesEnabled = areWorkletFeaturesEnabled(features);

        if (!workletFeaturesEnabled) return;

        const dspWasm = await this.#loader.loadDSPWasm();

        if (dspWasm instanceof LoadError) {
            return dspWasm;
        }

        this.#port.postMessage(
            {
                type: WORKLET_MESSAGES.RUNTIME_ASSETS,
                dspWasm,
            },
            [dspWasm],
        );

        this.#isDspLoaded = true;
    }

    async #loadWorkerFeatures(features: Features): Promise<void | LoadError> {
        if (this.#workerPort) return;

        const workerFeaturesEnabled = areWorkerFeaturesEnabled(features);

        if (!workerFeaturesEnabled) return;
        this.#initStartTime = performance.now();

        const { sampleRate } = super.context;
        const worker = await this.#loader.loadWorker();

        if (worker instanceof LoadError) {
            return worker;
        }
        this.#worker = worker;

        const workerWorkletChannel = new MessageChannel();
        this.#workerPort = this.#worker.createPort();

        this.#workerPort.postMessage(
            {
                type: WORKER_MESSAGES.INIT,
                workletPort: workerWorkletChannel.port1,
                sampleRate,
            },
            [workerWorkletChannel.port1],
        );
        this.#port.postMessage({ type: WORKLET_MESSAGES.WORKER_CREATED, workerPort: workerWorkletChannel.port2 }, [
            workerWorkletChannel.port2,
        ]);

        this.#workerPort.addEventListener('message', this.#handleWorkerMessage);
        this.#workerPort.start();
        this.#worker.addEventListener('error', this.#handleWorkerError);
    }

    #handleWorkletMessage: Exclude<WorkletPort['onmessage'], null> = (event): void => {
        if (this.#handleCommonMessage(event)) {
            return;
        }

        switch (event.data.type) {
            case CONTROLLER_MESSAGES.LOG:
                this.#logger[event.data.level](...event.data.data);

                return;
            case CONTROLLER_MESSAGES.ERROR:
                const { error } = event.data;
                this.#handleError(new SpenchError(error));

                return;
            case CONTROLLER_MESSAGES.LOW_PERFORMANCE:
                const potentialCause: LowPerformanceCause =
                    this.#currentMeanProcessTime === undefined
                        ? 'unknown'
                        : this.#currentMeanProcessTime > LP_CAUSE_THRESHOLD
                          ? 'inference'
                          : 'transport';

                if (!super.dispatchEvent(new LowPerformanceEvent(potentialCause, this.#currentMeanProcessTime))) {
                    this.#handleLowPerformance();
                }

                return;
            default:
        }

        assert(false, 'Unknown message type from AudioWorkletProcessor');
    };

    #handleWorkerMessage: Exclude<WorkerPort['onmessage'], null> = (event): void => {
        if (this.#handleCommonMessage(event)) {
            return;
        }

        assert(false, 'Unknown message type from Worker');
    };

    #handleLowPerformance() {
        // TODO HANDLE DEFAULT LOW PERFORMANCE
    }

    #handleError = (error: SpenchError) => {
        this.#logger.error(`${error} at SpenchAudioNode`);

        this.destroy();
    };

    #handleProcessorError = () => this.#handleError(new SpenchError('Error in AudioWorkletProcessor'));

    #handleWorkerError = (event: ErrorEvent) => this.#handleError(new SpenchError(event.error || event.message));

    #handleMessageError = () => this.#handleError(new SpenchError('Error serializing AudioWorkletProcessor message'));

    #handleContextStateChange = () => {
        switch (super.context.state) {
            case 'closed':
                this.destroy();
                break;
            case 'suspended':
                this.reset();
                break;
            default:
        }
    };

    #init(): void {
        super.context.addEventListener('statechange', this.#handleContextStateChange);
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
