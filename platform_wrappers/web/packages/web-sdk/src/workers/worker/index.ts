/* eslint-disable no-restricted-globals */

import {
    WORKER_MESSAGES,
    WORKLET_MESSAGES,
    ControllerToWorkerMessage,
    WorkerToWorkletMessage,
    WorkletToWorkerMessage,
    WorkerToControllerMessage,
} from '../../utils/ipc';
import { assert } from '../../utils/error';
import SpenchWrapper from '../../utils/wrapper';
import { getWorkerLogger } from '../../utils/logger';
import RingBuffer from '../../utils/ringBuffer';
import { getWorkerTelemetry } from '../../utils/telemetry';
import { TypedMessageEvent } from '../../utils/common';

interface SpenchWorkerGlobalScope extends DedicatedWorkerGlobalScope {
    onmessage: ((this: SpenchWorkerGlobalScope, event: TypedMessageEvent<ControllerToWorkerMessage>) => void) | null;

    postMessage(message: WorkerToControllerMessage, transfer: Transferable[]): void;
    postMessage(message: WorkerToControllerMessage, options?: StructuredSerializeOptions): void;
}

interface WorkletPort extends MessagePort {
    onmessage: ((this: WorkletPort, event: TypedMessageEvent<WorkletToWorkerMessage>) => void) | null;

    postMessage(message: WorkerToWorkletMessage, transfer: Transferable[]): void;
    postMessage(message: WorkerToWorkletMessage, options?: StructuredSerializeOptions): void;
}

const global = self as SpenchWorkerGlobalScope;
const logger = getWorkerLogger(global, '[worker]');
const telemetry = getWorkerTelemetry(global);

class SpenchWorker {
    readonly instance: SpenchWrapper;

    readonly workletPort: WorkletPort;

    readonly inputBuffer: RingBuffer;

    warmupComplete = false;

    constructor(wasm: ArrayBuffer, data: ArrayBuffer, workletPort: MessagePort, sampleRate: number) {
        this.workletPort = workletPort;

        SpenchWrapper.createBridge(wasm, data);
        this.instance = new SpenchWrapper('denoiser', sampleRate);
        this.inputBuffer = new RingBuffer(2 * this.instance.frameSize);

        logger.debug('ready');
        this.workletPort.onmessage = this.handleWorkletMessage;
        this.workletPort.postMessage({
            type: WORKLET_MESSAGES.WORKER_READY,
        });
    }

    destroy() {
        this.instance.destroy();
        this.workletPort.onmessage = null;
        this.workletPort.close();
    }

    handleWorkletMessage: WorkletPort['onmessage'] = (event) => {
        switch (event.data.type) {
            case WORKER_MESSAGES.INPUT:
                const { input } = event.data;

                this.inputBuffer.push(input);

                while (this.inputBuffer.size >= this.instance.frameSize) {
                    const startTime = performance.now();
                    const output = new Float32Array(this.instance.frameSize);
                    this.inputBuffer.pull(output, 0, this.instance.frameSize);
                    this.instance.process(output);
                    const endTime = performance.now();

                    this.workletPort.postMessage(
                        {
                            type: WORKLET_MESSAGES.OUTPUT,
                            output,
                        },
                        [output.buffer],
                    );

                    telemetry.recordFrameProcessTime(endTime - startTime);
                }

                return;
            case WORKER_MESSAGES.RESET:
                this.instance.resetContext();
                logger.debug('context reset');

                if (this.warmupComplete) {
                    telemetry.flush();
                } else {
                    this.warmupComplete = true;
                }

                return;
            default:
        }

        assert(false, 'Unknown message type from Worklet');
    };
}

let runtime: SpenchWorker | null = null;

const handleControllerMessage: SpenchWorkerGlobalScope['onmessage'] = (event) => {
    switch (event.data.type) {
        case WORKER_MESSAGES.INIT:
            logger.debug('init data received');

            const { wasm, data, workletPort, sampleRate } = event.data;
            runtime = new SpenchWorker(wasm, data, workletPort, sampleRate);

            return;
        case WORKER_MESSAGES.DESTROY:
            runtime?.destroy();
            runtime = null;

            logger.debug('destroyed');

            global.onmessage = null;
            global.close();

            return;
        default:
    }

    assert(false, 'Unknown message type from Controller');
};

global.onmessage = handleControllerMessage;
