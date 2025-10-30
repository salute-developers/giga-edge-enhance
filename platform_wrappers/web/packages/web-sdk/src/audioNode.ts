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
import {
    areMainFeaturesEnabled,
    areDspFeaturesEnabled,
    Features,
    getDspFeatures,
    Settings,
    DspFeatures,
    areDspFeaturesChanged,
    areMainFeaturesChanged,
} from './utils/common';
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
    setFeatures(features: Partial<Features>): Promise<void | LoadError | SetFeaturesFailedError>;
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

    #features: Features = {
        normalization: false,
        echoCancellation: false,
        noiseCancellation: false,
        dereverberation: false,
    };

    #worker: SpenchWorker | undefined;

    #workerPort: WorkerPort | undefined;

    #initialized = false;

    #isDspLoaded = false;

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

    get features(): Features {
        return this.#features;
    }

    get #port(): WorkletPort {
        return super.port;
    }

    async setFeatures(features: Partial<Features>): Promise<void | LoadError | SetFeaturesFailedError> {
        if (this.#setFeaturesLock) {
            return this.#setFeaturesLock.then(() => this.setFeatures(features));
        }
        const { promise, resolve } = new Deferred<void>();
        this.#setFeaturesLock = promise;

        const filteredFeatures = Object.fromEntries(
            Object.entries(features).filter(([, value]) => value !== undefined),
        );
        this.#logger.info('Setting Spench features:', filteredFeatures);

        const mixed: Features = {
            ...this.#features,
            ...filteredFeatures,
        };
        let res;

        if (this.#settings.dspOnAudioThread) {
            if (areMainFeaturesEnabled(mixed)) {
                this.#logger.warn('Main features are not available when dspOnAudioThread is enabled');
            }

            res = await this.#setDspWorkletFeatures(mixed);
        } else {
            res = await this.#setFeatures(mixed);
        }

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
        const workerFeaturesEnabled = areMainFeaturesEnabled(features) || areDspFeaturesEnabled(features);
        const workerSetResult = await this.#setWorkerFeatures(features);

        if (workerSetResult instanceof SpenchError) {
            return workerSetResult;
        }

        this.#port.postMessage({
            type: WORKLET_MESSAGES.SET_FEATURES,
            workerFeaturesEnabled,
        });

        if (!this.#initialized && (this.#features.noiseCancellation || this.#features.dereverberation)) {
            this.#initialized = true;
            this.#telemetry?.sendMetric('initTime', performance.now() - this.#initStartTime);
        }
    }

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
                    resolve(new SetFeaturesFailedError(error));
                } else {
                    this.#logger.debug('DSP worklet features applied');
                    resolve();
                }

                this.#port.removeEventListener('message', handler);
            };

            this.#port.addEventListener('message', handler);
        });
    }

    async #setWorkerFeatures(features: Features): Promise<void | LoadError | SetFeaturesFailedError> {
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
        this.#workerPort!.postMessage({ type: WORKER_MESSAGES.SET_FEATURES, features });

        return new Promise<void | LoadError | SetFeaturesFailedError>((resolve) => {
            const handler = (ev: MessageEvent<ControllerMessage>) => {
                if (ev.data.type !== CONTROLLER_MESSAGES.SET_FEATURES_RESULT) {
                    return;
                }

                const { errors, features: resultFeatures } = ev.data;

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
                    resolve(new SetFeaturesFailedError(criticalError));
                } else if (loadError) {
                    resolve(new LoadError(loadError));
                } else {
                    this.#logger.debug('Features applied', resultFeatures);
                    resolve();
                }

                this.#workerPort!.removeEventListener('message', handler);
            };

            this.#workerPort!.addEventListener('message', handler);
        });
    }

    #handleCommonMessage(event: MessageEvent<ControllerMessage>): boolean {
        switch (event.data.type) {
            case CONTROLLER_MESSAGES.SET_FEATURES_RESULT:
                return true;
            default:
        }

        return false;
    }

    async #loadWorker(): Promise<void | LoadError> {
        if (this.#workerPort) return;

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
            case CONTROLLER_MESSAGES.TELEMETRY:
                this.#telemetry?.sendMetric(event.data.metricName, event.data.value);

                return;
            case CONTROLLER_MESSAGES.ERROR:
                const { error } = event.data;
                this.#handleError(new SpenchError(error));

                return;
            case CONTROLLER_MESSAGES.LOW_PERFORMANCE:
                const potentialCause: LowPerformanceCause =
                    this.#telemetry.currentMeanProcessTime === undefined
                        ? 'unknown'
                        : this.#telemetry.currentMeanProcessTime > LP_CAUSE_THRESHOLD
                          ? 'inference'
                          : 'transport';

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
