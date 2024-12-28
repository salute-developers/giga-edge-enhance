import { Logger } from './utils/logger';
import { AudioNodeTelemetry } from './utils/telemetry';
import SpenchError, { assert } from './utils/error';
import {
    CommonToControllerMessage,
    CONTROLLER_MESSAGES,
    ControllerToWorkerMessage,
    ControllerToWorkletMessage,
    WORKER_MESSAGES,
    WorkerToControllerMessage,
    WORKLET_MESSAGES,
    WorkletToControllerMessage,
} from './utils/ipc';
import { Features, TypedMessageEvent } from './utils/common';

// Normalizer — deprecated
// TODO unify
type ProcessorKind = 'processor' | 'normalizer';

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

// eslint-disable-next-line @typescript-eslint/no-empty-object-type
interface SpenchWorker extends Worker {
    onmessage: ((this: SpenchWorker, event: TypedMessageEvent<WorkerToControllerMessage>) => void) | null;

    postMessage(message: ControllerToWorkerMessage, transfer: Transferable[]): void;
    postMessage(message: ControllerToWorkerMessage, options?: StructuredSerializeOptions): void;
}

interface WorkletPort extends MessagePort {
    onmessage: ((this: WorkletPort, event: TypedMessageEvent<WorkletToControllerMessage>) => void) | null;

    postMessage(message: ControllerToWorkletMessage, transfer: Transferable[]): void;
    postMessage(message: ControllerToWorkletMessage, options?: StructuredSerializeOptions): void;
}

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

interface SpenchAudioNodeBaseCtorParams {
    kind: ProcessorKind;
    features: Features;
    ctx: AudioContext;
    logger: Logger;
    telemetry: AudioNodeTelemetry;

    wasm: ArrayBuffer;
}

interface SpenchAudioNodeNormCtorParams extends SpenchAudioNodeBaseCtorParams {
    kind: 'normalizer';
    features: { normalization: true };
}

interface SpenchAudioNodeMainCtorParams extends SpenchAudioNodeBaseCtorParams {
    kind: 'processor';
    features: Features;

    data: ArrayBuffer;
    worker: Worker;
}

type SpenchAudioNodeCtorParams = SpenchAudioNodeNormCtorParams | SpenchAudioNodeMainCtorParams;

export class SpenchAudioNodeImpl extends AudioWorkletNode implements SpenchAudioNode {
    readonly #kind: ProcessorKind;

    readonly #logger: Logger;

    #features: Features;

    // TODO SharedArrayBuffer
    #worker: SpenchWorker | undefined;

    #initialized = false;

    #initStartTime = performance.now();

    #telemetry: AudioNodeTelemetry;

    constructor({ kind, features, ctx, wasm, logger, telemetry, ...params }: SpenchAudioNodeCtorParams) {
        const processorName = `spench-${kind}`;
        super(ctx, processorName);

        this.#kind = kind;
        this.#features = features;
        this.#logger = logger;
        this.#telemetry = telemetry;
        this.#worker = 'worker' in params ? params.worker : undefined;

        this.#attachListeners();
        this.#init(wasm, 'data' in params ? params.data : undefined);
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

        if (this.#worker) {
            this.#logger.debug('Destroying Spench Worker');

            this.#worker.postMessage({ type: WORKER_MESSAGES.DESTROY });
            this.#worker.onmessage = null;
            this.#worker.onerror = null;
            this.#worker.onmessageerror = null;
            this.#worker = undefined;
        }

        this.#port.postMessage({ type: WORKLET_MESSAGES.DESTROY });
        this.#port.onmessage = null;
        this.#port.onmessageerror = null;
        this.#port.close();

        super.onprocessorerror = null;
        super.context.removeEventListener('statechange', this.#handleContextStateChange);

        super.dispatchEvent(new Event('destroyed'));
    }

    #setFeatures(features: Features, _manual = false): void {
        if (this.#kind !== 'processor') {
            return;
        }
        assert(this.#worker, 'Worker should be present');

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

            this.port.postMessage({ type: WORKLET_MESSAGES.WORKER_EJECTED });
            this.#worker.postMessage({ type: WORKER_MESSAGES.SET_FEATURES, features });
        }
    }

    #handleCommonMessage(event: TypedMessageEvent<CommonToControllerMessage>): boolean {
        switch (event.data.type) {
            case CONTROLLER_MESSAGES.LOG:
                this.#logger[event.data.level](...event.data.data);

                return true;
            case CONTROLLER_MESSAGES.TELEMETRY:
                this.#telemetry.sendMetric(event.data.metricName, event.data.value);

                return true;
            default:
        }

        return false;
    }

    #handleWorkletMessage: WorkletPort['onmessage'] = (event): void => {
        if (this.#handleCommonMessage(event as TypedMessageEvent<CommonToControllerMessage>)) {
            return;
        }

        switch (event.data.type) {
            case CONTROLLER_MESSAGES.OVERFLOW:
                if (!super.dispatchEvent(new OverflowEvent({ overflowCount: event.data.overflowCount }))) {
                    this.#handleOverflow(event.data.overflowCount);
                }

                return;
            case CONTROLLER_MESSAGES.SYSTEM_READY:
                if (!this.#initialized) {
                    this.#initialized = true;
                    this.#telemetry.sendMetric('initTime', performance.now() - this.#initStartTime);
                }
                super.dispatchEvent(new Event('ready'));

                return;
            default:
        }

        assert(false, 'Unknown message type from AudioWorkletProcessor');
    };

    #handleWorkerMessage: SpenchWorker['onmessage'] = (event): void => {
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

    #init(wasm: ArrayBuffer, data?: ArrayBuffer): void {
        const { sampleRate } = super.context;

        if (this.#kind === 'processor') {
            const workerWorkletChannel = new MessageChannel();

            assert(this.#worker, 'Worker should be present');
            assert(data, 'Model Data should be present');

            this.#worker.postMessage(
                {
                    type: WORKER_MESSAGES.INIT,
                    workletPort: workerWorkletChannel.port1,
                    features: this.#features,
                    wasm,
                    data,
                    sampleRate,
                },
                [workerWorkletChannel.port1, wasm, data],
            );
            this.#port.postMessage(
                {
                    type: WORKLET_MESSAGES.INIT,
                    workerPort: workerWorkletChannel.port2,
                },
                [workerWorkletChannel.port2],
            );
        } else if (this.#kind === 'normalizer') {
            this.#port.postMessage({ type: WORKLET_MESSAGES.INIT_NORM, wasm }, [wasm]);
        } else {
            assert(false, 'Unknown AudioWorkletProcessor kind');
        }
    }

    #attachListeners(): void {
        super.context.addEventListener('statechange', this.#handleContextStateChange);
        super.onprocessorerror = () => this.#handleError(new SpenchError('Error in AudioWorkletProcessor'));

        this.#port.onmessage = this.#handleWorkletMessage;
        this.#port.onmessageerror = () =>
            this.#handleError(new SpenchError('Error serializing AudioWorkletProcessor message'));

        if (this.#worker) {
            this.#worker.onmessage = this.#handleWorkerMessage;
            this.#worker.onerror = (event) =>
                this.#handleError(new SpenchError(event.error || event.message), event.message);
            this.#worker.onmessageerror = () => this.#handleError(new SpenchError('Error serializing Worker message'));
        }
    }
}
