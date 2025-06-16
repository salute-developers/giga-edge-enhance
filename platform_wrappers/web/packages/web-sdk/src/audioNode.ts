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
import { Features } from './utils/common';
import { SPENCH_WORKLET_NAME } from './utils/constants';
import SpenchWorker from './worker';
import Loader, { LoadError } from './loader';
import Deferred from './utils/deferred';

type OverflowEventCtorParams = {
    overflowCount: number;
};

class SetFeaturesFailedError extends SpenchError {}

class OverflowEvent extends Event {
    overflowCount: number;

    constructor({ overflowCount }: OverflowEventCtorParams) {
        super('overflow', { cancelable: true });

        this.overflowCount = overflowCount;
    }
}

interface SpenchAudioNodeEventMap {
    ready: Event;
    error: ErrorEvent;
    overflow: OverflowEvent;
    destroyed: Event;
}

type WorkletPort = CustomPort<ControllerMessage, WorkletMessage>;
type WorkerPort = CustomPort<ControllerMessage, WorkerMessage>;

export interface SpenchAudioNode extends AudioWorkletNode {
    enable(): void;
    disable(): void;
    setFeatures(features: Features): void;
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
    features: Features;
    logger: Logger;
    ctx: AudioContext;
    loader: Loader;
    telemetry: Telemetry;
}

export class SpenchAudioNodeImpl extends AudioWorkletNode implements SpenchAudioNode {
    readonly #logger: Logger;

    readonly #loader: Loader;

    readonly #telemetry: Telemetry;

    #features: Features = {};

    #worker: SpenchWorker | undefined;

    #workerPort: WorkerPort | undefined;

    #initialized = false;

    #isDspLoaded = false;

    #setFeaturesLock: Promise<void> | undefined;

    #initStartTime = performance.now();

    constructor({ features, ctx, logger, telemetry, loader }: SpenchAudioNodeCtorParams) {
        super(ctx, SPENCH_WORKLET_NAME);

        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#loader = loader;

        this.#init();
        this.setFeatures(features);
    }

    get #port(): WorkletPort {
        return super.port;
    }

    enable(): void {
        this.#logger.debug('Enabling Spench AudioNode');

        this.#port.postMessage({ type: WORKLET_MESSAGES.ENABLE });
    }

    disable(): void {
        this.#logger.debug('Disabling Spench AudioNode');

        this.#port.postMessage({ type: WORKLET_MESSAGES.DISABLE });
    }

    async setFeatures(features: Features): Promise<void> {
        await this.#setFeatures(features, true);
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

    async #setFeatures(features: Features, _manual = false): Promise<void | LoadError | SetFeaturesFailedError> {
        if (this.#setFeaturesLock) {
            return this.#setFeaturesLock.then(() => this.#setFeatures(features, _manual));
        }
        const { promise, resolve } = new Deferred<void>();
        this.#setFeaturesLock = promise;

        this.#logger.info('Setting Spench features:', features);
        const res = await Promise.all([this.#setWorkletFeatures(features), this.#setWorkerFeatures(features)]);
        const err = res.find((r) => r instanceof SpenchError);

        this.#setFeaturesLock = undefined;
        resolve();

        if (err) return err;
    }

    async #setWorkletFeatures(features: Features): Promise<void | LoadError | SetFeaturesFailedError> {
        if (features.echoCancellation) {
            this.#logger.warn('echoCancellation feature is not implemented yet, so it will be ignored');
        }

        const areWorkletFeaturesEnabled = [
            features.normalization,
            // features.echoCancellation, TODO ADD AEC FEATURE
        ].some(Boolean);

        // Always rebuild normalizer
        const areWorkletFeaturesChanged = [
            features.normalization === undefined ? this.#features.normalization : features.normalization,
            // typeof features.echoCancellation !== 'undefined' &&
            //     this.#features.echoCancellation !== features.echoCancellation,
        ].some(Boolean);
        const mixedFeatures = {
            ...this.#features,
            ...features,
        };
        const {
            normalization,
            // echoCancellation,
        } = mixedFeatures;
        const workletFeatures = {
            normalization,
            // echoCancellation,
        };

        if (areWorkletFeaturesEnabled) {
            const res = await this.#loadWorkletFeatures();

            if (res instanceof LoadError) {
                return res;
            }
        }

        if (areWorkletFeaturesChanged) {
            this.#logger.debug('Applying Worklet features:', features);
            this.#port.postMessage({ type: WORKLET_MESSAGES.SET_FEATURES, features: workletFeatures });

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

    async #setWorkerFeatures(features: Features): Promise<void | LoadError | SetFeaturesFailedError> {
        const areWorkerFeaturesEnabled = [features.noiseCancellation, features.dereverberation].some(Boolean);
        // TODO false and undefined are equal in terms of starting woker
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
        const { noiseCancellation, dereverberation } = mixedFeatures;
        const workerFeatures = { noiseCancellation, dereverberation };

        if (areWorkerFeaturesEnabled) {
            const res = await this.#loadWorkerFeatures();
            this.#initStartTime = performance.now();

            if (res instanceof LoadError) {
                return res;
            }
        }

        if (areWorkerFeaturesChanged) {
            this.#logger.debug('Applying Worker features:', features);
            this.#port.postMessage({
                type: WORKLET_MESSAGES.WORKER_FEATURES_CHANGED,
                enabled: areWorkerFeaturesEnabled,
            });
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
                this.#telemetry?.sendMetric(event.data.metricName, event.data.value);

                return true;
            case CONTROLLER_MESSAGES.SET_FEATURES_OK:
            case CONTROLLER_MESSAGES.SET_FEATURES_FAIL:
                return true;
            default:
        }

        return false;
    }

    async #loadWorkletFeatures(): Promise<void | LoadError> {
        if (this.#isDspLoaded) return;

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
    }

    async #loadWorkerFeatures(): Promise<void | LoadError> {
        if (this.#workerPort) return;

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
            case CONTROLLER_MESSAGES.OVERFLOW:
                if (!super.dispatchEvent(new OverflowEvent({ overflowCount: event.data.overflowCount }))) {
                    this.#handleOverflow(event.data.overflowCount);
                }

                return;
            case CONTROLLER_MESSAGES.SYSTEM_READY:
                if (!this.#initialized && (this.#features.noiseCancellation || this.#features.dereverberation)) {
                    this.#initialized = true;
                    this.#telemetry?.sendMetric('initTime', performance.now() - this.#initStartTime);
                }
                super.dispatchEvent(new Event('ready'));

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

    #handleOverflow(_count: number) {
        // TODO HANDLE DEFAULT OVERFLOW
        // Based on latency (frame distance)
        // now — based on frame drop (just signal n samples dropped per frame)
    }

    #handleError = (error: SpenchError, message: string = '') => {
        this.#logger.error(`${error} at SpenchAudioNode`);

        this.destroy();
        super.dispatchEvent(new ErrorEvent('error', { error, message: message || error.message }));
    };

    #handleProcessorError = () => this.#handleError(new SpenchError('Error in AudioWorkletProcessor'));

    #handleWorkerError = (event: ErrorEvent) =>
        this.#handleError(new SpenchError(event.error || event.message), event.message);

    #handleMessageError = () => this.#handleError(new SpenchError('Error serializing AudioWorkletProcessor message'));

    #handleContextStateChange = () => {
        if (super.context.state === 'closed') {
            this.destroy();
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
        });
    }
}
