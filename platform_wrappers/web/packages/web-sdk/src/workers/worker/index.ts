/* eslint-disable no-restricted-globals */

import {
    WORKER_MESSAGES,
    WORKLET_MESSAGES,
    SELF_WORKER_MESSAGES,
    CustomPort,
    WorkerMessage,
    WorkletMessage,
    SelfWorkerMessage,
    WorkerControllerMessage,
    ControllerMessage,
} from '../../utils/ipc';
import { assert } from '../../utils/error';
import SpenchWrapper from '../../utils/wrapper';
import { getWorkerLogger } from '../../utils/logger';
import { getWorkerTelemetry, WorkerTelemetry } from '../../utils/telemetry';
import { Features } from '../../utils/common';
import { SPENCH_FRAME_SIZE_SECONDS, TRANSPORT_SIZE_SECONDS, WARMUP_STEP_COUNT, WASM_PATH } from '../../utils/constants';
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
const logger = getWorkerLogger(global, '[worker]');
let isTelemetryEnabled = false;

class SpenchWorker {
    private readonly instance: SpenchWrapper;

    private readonly telemetry: WorkerTelemetry;

    private readonly inputBuffer: RingBuffer;

    private readonly outputBuffer: RingBuffer;

    constructor(
        private readonly controllerPort: ControllerPort,
        private readonly workletPort: WorkletPort,
        readonly sampleRate: number,
        features: Features,
    ) {
        this.instance = new SpenchWrapper(sampleRate, logger);
        this.telemetry = getWorkerTelemetry(isTelemetryEnabled, controllerPort);
        this.inputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));
        this.outputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));
        this.workletPort.onmessage = this.handleWorkletMessage;
        this.controllerPort.onmessage = this.handleControllerMessage;

        this.setFeatures(features);
    }

    destroy() {
        this.telemetry.flush();
        this.instance.destroy();
        this.workletPort.onmessage = null;
        this.workletPort.close();
        this.controllerPort.onmessage = null;
        this.controllerPort.close();
    }

    setFeatures(features: Features): void {
        logger.debug(`setting features: ${JSON.stringify(features)}`);
        this.instance.clear();

        const hasHeavyComputation = Boolean(features.noiseCancellation);

        if (features.echoCancellation) {
            const model = features.echoCancellation === true ? 'full48k' : features.echoCancellation;
            this.instance.addDevererberator(WASM_PATH.dereverb[model]);
        }

        if (features.noiseCancellation) {
            const model = features.noiseCancellation === true ? 'full24k' : features.noiseCancellation;
            this.instance.addDenoiser(WASM_PATH.denoiser[model]);
        }

        if (features.normalization) {
            this.instance.addNormalizer();
        }

        if (hasHeavyComputation) {
            this.warmup();
            this.instance.resetContext();
        }

        this.telemetry.enabled = hasHeavyComputation && isTelemetryEnabled;

        this.workletPort.postMessage({
            type: WORKLET_MESSAGES.WORKER_READY,
        });
        logger.debug('ready');
    }

    warmup() {
        for (let stepsCount = 0; stepsCount < WARMUP_STEP_COUNT; ++stepsCount) {
            this.instance.process(new Float32Array(128));
        }
    }

    handleWorkletMessage: WorkletPort['onmessage'] = (event) => {
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

                    this.telemetry.recordFrameProcessTime(endTime - startTime);

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
            case WORKER_MESSAGES.RESET:
                this.instance.resetContext();
                logger.debug('context reset');
                this.telemetry.flush();

                return;
            default:
        }

        assert(false, 'Unknown message type from Worklet');
    };

    handleControllerMessage: ControllerPort['onmessage'] = (event) => {
        switch (event.data.type) {
            case WORKER_MESSAGES.SET_FEATURES:
                const { features } = event.data;
                this.setFeatures(features);

                return;
            case WORKER_MESSAGES.DESTROY:
                this.destroy();
                instances.delete(this);

                return;
            default:
        }

        assert(false, 'Unknown message type from Worklet');
    };
}

const handleControllerMessage: SpenchWorkerGlobalScope['onmessage'] = (event) => {
    switch (event.data.type) {
        case SELF_WORKER_MESSAGES.INIT:
            logger.debug('init data received');

            const { isDebug, isTelemetryEnabled: telemetryEnabled, wasm, data } = event.data;
            SpenchWrapper.createBridge(wasm, data);

            // TODO better debug log and telemetry cancelling
            if (!isDebug) {
                logger.debug = () => {};
            }
            isTelemetryEnabled = telemetryEnabled;

            return;
        case SELF_WORKER_MESSAGES.NEW_CLIENT:
            const { port: controllerPort } = event.data;

            controllerPort.addEventListener(
                'message',
                (e) => {
                    assert(e.data.type === WORKER_MESSAGES.INIT);
                    const { features, sampleRate, workletPort } = e.data;

                    instances.add(new SpenchWorker(controllerPort, workletPort, sampleRate, features));
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

            global.onmessage = null;
            global.close();

            return;
        default:
    }

    assert(false, 'Unknown message type from Controller');
};

global.onmessage = handleControllerMessage;
