import { Logger } from './utils/logger';
import { AudioNodeTelemetry } from './utils/telemetry';
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

type OverflowEventCtorParams = {
    overflowCount: number;
};

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
    ctx: AudioContext;
    logger: Logger;
    worker: SpenchWorker;
    isDebug: boolean;
    telemetry?: AudioNodeTelemetry;
}

export class SpenchAudioNodeImpl extends AudioWorkletNode implements SpenchAudioNode {
    readonly #logger: Logger;

    #features: Features;

    // TODO SharedArrayBuffer
    #worker: SpenchWorker;

    #workerPort: WorkerPort;

    #initialized = false;

    #initStartTime = performance.now();

    #telemetry: AudioNodeTelemetry | undefined;

    constructor({ isDebug, features, ctx, logger, telemetry, worker }: SpenchAudioNodeCtorParams) {
        super(ctx, SPENCH_WORKLET_NAME);

        this.#features = features;
        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#worker = worker;
        this.#workerPort = this.#worker.createPort();

        this.#attachListeners();
        this.#init(isDebug);
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

    setFeatures(features: Features): void {
        this.#setFeatures(features, true);
    }

    destroy(): void {
        this.#logger.debug('Destroying Spench AudioNode');

        this.#workerPort.close();
        this.#worker.onerror = null;

        this.#port.postMessage({ type: WORKLET_MESSAGES.DESTROY });
        this.#port.onmessage = null;
        this.#port.onmessageerror = null;
        this.#port.close();

        super.onprocessorerror = null;
        super.context.removeEventListener('statechange', this.#handleContextStateChange);

        super.dispatchEvent(new Event('destroyed'));
    }

    #setFeatures(features: Features, _manual = false): void {
        const isChanged = [
            typeof features.normalization !== 'undefined' && this.#features.normalization !== features.normalization,
            typeof features.noiseCancellation !== 'undefined' &&
                this.#features.noiseCancellation !== features.noiseCancellation,
            typeof features.echoCancellation !== 'undefined' &&
                this.#features.echoCancellation !== features.echoCancellation,
        ].some(Boolean);

        if (isChanged) {
            this.#features = {
                ...this.#features,
                ...features,
            };

            this.#port.postMessage({ type: WORKLET_MESSAGES.WORKER_EJECTED });
            this.#workerPort.postMessage({ type: WORKER_MESSAGES.SET_FEATURES, features });
        }
    }

    #handleCommonMessage(event: MessageEvent<ControllerMessage>): boolean {
        switch (event.data.type) {
            case CONTROLLER_MESSAGES.TELEMETRY:
                this.#telemetry?.sendMetric(event.data.metricName, event.data.value);

                return true;
            default:
        }

        return false;
    }

    #handleWorkletMessage: WorkletPort['onmessage'] = (event): void => {
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
                if (!this.#initialized) {
                    this.#initialized = true;
                    this.#telemetry?.sendMetric('initTime', performance.now() - this.#initStartTime);
                }
                super.dispatchEvent(new Event('ready'));

                return;
            default:
        }

        assert(false, 'Unknown message type from AudioWorkletProcessor');
    };

    #handleWorkerMessage: WorkerPort['onmessage'] = (event): void => {
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

    #handleContextStateChange = () => {
        if (super.context.state === 'closed') {
            this.destroy();
        }
    };

    #init(isDebug: boolean): void {
        const { sampleRate } = super.context;

        const workerWorkletChannel = new MessageChannel();

        this.#workerPort.postMessage(
            {
                type: WORKER_MESSAGES.INIT,
                workletPort: workerWorkletChannel.port1,
                features: this.#features,
                sampleRate,
            },
            [workerWorkletChannel.port1],
        );
        this.#port.postMessage(
            {
                type: WORKLET_MESSAGES.INIT,
                workerPort: workerWorkletChannel.port2,
                isDebug,
                isTelemetryEnabled: Boolean(this.#telemetry),
            },
            [workerWorkletChannel.port2],
        );
    }

    #attachListeners(): void {
        super.context.addEventListener('statechange', this.#handleContextStateChange);
        super.onprocessorerror = () => this.#handleError(new SpenchError('Error in AudioWorkletProcessor'));

        this.#port.onmessage = this.#handleWorkletMessage;
        this.#port.onmessageerror = () =>
            this.#handleError(new SpenchError('Error serializing AudioWorkletProcessor message'));

        this.#workerPort.onmessage = this.#handleWorkerMessage;
        this.#worker.onerror = (event) =>
            this.#handleError(new SpenchError(event.error || event.message), event.message);
    }
}
