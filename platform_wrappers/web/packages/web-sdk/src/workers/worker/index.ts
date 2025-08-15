/* eslint-disable no-restricted-globals */

import {
    WORKER_MESSAGES,
    WORKLET_MESSAGES,
    CONTROLLER_MESSAGES,
    SELF_WORKER_MESSAGES,
    CustomPort,
    WorkerMessage,
    WorkletMessage,
    SelfWorkerMessage,
    WorkerControllerMessage,
    ControllerMessage,
} from '../../utils/ipc';
import { assert } from '../../utils/error';
import { SpenchWrapper } from '../../utils/wrapper';
import { getWorkerLogger } from '../../utils/logger';
import { getWorkerTelemetry, WorkerTelemetry } from '../../utils/telemetry';
import { WorkerFeatures } from '../../utils/common';
import { SPENCH_FRAME_SIZE_SECONDS, TRANSPORT_SIZE_SECONDS, WASM_PATH } from '../../utils/constants';
import RingBuffer from '../../utils/ringBuffer';

interface SpenchWorkerGlobalScope extends DedicatedWorkerGlobalScope {
    onmessage: ((this: SpenchWorkerGlobalScope, event: MessageEvent<SelfWorkerMessage>) => void) | null;

    addEventListener<M extends { message: MessageEvent<SelfWorkerMessage> }, K extends keyof M>(
        type: K,
        listener: (this: SpenchWorkerGlobalScope, ev: M[K]) => any,
        options?: boolean | AddEventListenerOptions,
    ): void;
    removeEventListener<M extends { message: MessageEvent<SelfWorkerMessage> }, K extends keyof M>(
        type: K,
        listener: (this: SpenchWorkerGlobalScope, ev: M[K]) => any,
        options?: boolean | EventListenerOptions,
    ): void;

    postMessage(message: WorkerControllerMessage, transfer: Transferable[]): void;
    postMessage(message: WorkerControllerMessage, options?: StructuredSerializeOptions): void;
}

type WorkletPort = CustomPort<WorkerMessage, WorkletMessage>;
type ControllerPort = CustomPort<WorkerMessage, ControllerMessage>;

const global = self as SpenchWorkerGlobalScope;
const instances = new Set<SpenchWorker>();
let logger = getWorkerLogger(global, 'verbose', '[worker]');
let isTelemetryEnabled = false;

class SpenchWorker {
    private readonly instance: SpenchWrapper;

    private readonly telemetry: WorkerTelemetry;

    private readonly inputBuffer: RingBuffer;

    private readonly outputBuffer: RingBuffer;

    constructor(
        private readonly controllerPort: ControllerPort,
        private readonly workletPort: WorkletPort,
        private readonly sampleRate: number,
    ) {
        this.instance = new SpenchWrapper(sampleRate, logger);
        this.telemetry = getWorkerTelemetry(isTelemetryEnabled, controllerPort);
        this.inputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));
        this.outputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));
        this.workletPort.addEventListener('message', this.handleWorkletMessage);
        this.controllerPort.addEventListener('message', this.handleControllerMessage);
        this.workletPort.start();
        this.controllerPort.start();
    }

    destroy() {
        this.telemetry.flush();
        this.instance.destroy();
        this.workletPort.close();
        this.controllerPort.close();
    }

    reset() {
        this.instance.resetContext();
        this.inputBuffer.clear();
        this.outputBuffer.clear();
        this.telemetry.flush();

        logger.debug('context reset');
    }

    setFeatures(features: WorkerFeatures): void {
        logger.debug(`setting features: ${JSON.stringify(features)}`);
        this.instance.clear();

        if (features.dereverberation) {
            const model = features.dereverberation === true ? 'full48k' : features.dereverberation;
            this.instance.addDereverberator(WASM_PATH.dereverb[model]);
        }

        if (features.noiseCancellation) {
            const model = features.noiseCancellation === true ? 'full24k' : features.noiseCancellation;
            this.instance.addDenoiser(WASM_PATH.denoiser[model]);
        }
    }

    withErrorHandling = <T extends (...args: any[]) => any>(fn: T): T => {
        // eslint-disable-next-line @typescript-eslint/no-this-alias
        const origThis = this;

        return function wrapped(this: ThisParameterType<T>, ...args: Parameters<T>): ReturnType<T> | void {
            try {
                return fn.apply(this, args);
            } catch (e) {
                origThis.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.ERROR, error: e as Error });
            }
        } as T;
    };

    // eslint-disable-next-line @typescript-eslint/member-ordering
    handleWorkletMessage: Exclude<WorkletPort['onmessage'], null> = this.withErrorHandling((event) => {
        switch (event.data.type) {
            case WORKER_MESSAGES.INPUT:
                const { input } = event.data;
                const frameSize = Math.ceil(SPENCH_FRAME_SIZE_SECONDS * this.sampleRate);

                this.inputBuffer.push(input);

                while (this.inputBuffer.size >= frameSize) {
                    const frame = new Float32Array(frameSize);
                    this.inputBuffer.pull(frame, 0, frameSize);

                    const startTime = performance.now();
                    const output = this.instance.process(frame);
                    const endTime = performance.now();

                    if (!output.length) {
                        continue;
                    }

                    const elapsed = endTime - startTime;
                    this.telemetry.recordFrameProcessTime(elapsed);
                    this.telemetry.recordMeanFrameProcessTime(elapsed);

                    this.outputBuffer.push(output);
                    const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * this.sampleRate);
                    const chunksToSend = Math.trunc(this.outputBuffer.size / transferChunkSize);

                    if (chunksToSend > 0) {
                        const chunk = new Float32Array(transferChunkSize * chunksToSend);
                        this.outputBuffer.pull(chunk, 0, transferChunkSize * chunksToSend);

                        this.workletPort.postMessage(
                            {
                                type: WORKLET_MESSAGES.OUTPUT,
                                output: chunk,
                            },
                            [chunk.buffer],
                        );
                    }
                }

                return;
            default:
        }

        assert(false, 'Unknown message type from Worklet');
    });

    // eslint-disable-next-line @typescript-eslint/member-ordering
    handleControllerMessage: Exclude<ControllerPort['onmessage'], null> = this.withErrorHandling((event) => {
        switch (event.data.type) {
            case WORKER_MESSAGES.SET_FEATURES:
                const { features } = event.data;

                try {
                    this.setFeatures(features);
                } catch (e) {
                    this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SET_FEATURES_FAIL, error: e as Error });

                    return;
                }
                this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SET_FEATURES_OK });

                return;
            case WORKER_MESSAGES.DESTROY:
                this.destroy();
                instances.delete(this);

                return;
            case WORKER_MESSAGES.RESET:
                this.reset();

                return;
            default:
        }

        assert(false, 'Unknown message type from Worklet');
    });
}

const handleControllerMessage: SpenchWorkerGlobalScope['onmessage'] = (event) => {
    switch (event.data.type) {
        case SELF_WORKER_MESSAGES.INIT:
            const { verbosity, isTelemetryEnabled: telemetryEnabled, wasm, data } = event.data;
            SpenchWrapper.createBridge(wasm, data, logger);

            logger = getWorkerLogger(global, verbosity, '[worker]');
            isTelemetryEnabled = telemetryEnabled;
            logger.debug('init data received');

            return;
        case SELF_WORKER_MESSAGES.NEW_CLIENT:
            const { port: controllerPort } = event.data;

            controllerPort.addEventListener(
                'message',
                (e) => {
                    assert(e.data.type === WORKER_MESSAGES.INIT);
                    const { sampleRate, workletPort } = e.data;

                    instances.add(new SpenchWorker(controllerPort, workletPort, sampleRate));
                },
                { once: true },
            );
            controllerPort.start();

            return;
        case SELF_WORKER_MESSAGES.DESTROY:
            for (const instance of instances) {
                instance.destroy();
            }

            logger.debug('destroyed');

            global.close();

            return;
        default:
    }

    assert(false, 'Unknown message type from Controller');
};

global.addEventListener('message', handleControllerMessage);
