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
import { getWorkerTelemetry } from '../../utils/telemetry';
import { Features, TypedMessageEvent } from '../../utils/common';
import { SPENCH_FRAME_SIZE_SECONDS, WARMUP_STEP_COUNT, WASM_PATH } from '../../utils/constants';
import RingBuffer from '../../utils/ringBuffer';

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

    readonly buffer: RingBuffer;

    constructor(
        wasm: ArrayBuffer,
        data: ArrayBuffer,
        readonly workletPort: WorkletPort,
        readonly sampleRate: number,
        features: Features,
    ) {
        SpenchWrapper.createBridge(wasm, data);
        this.instance = new SpenchWrapper(sampleRate, logger);
        this.buffer = new RingBuffer(2 * SPENCH_FRAME_SIZE_SECONDS * sampleRate);
        this.workletPort.onmessage = this.handleWorkletMessage;

        this.setFeatures(features);
    }

    destroy() {
        this.instance.destroy();
        this.workletPort.onmessage = null;
        this.workletPort.close();
    }

    setFeatures(features: Features): void {
        logger.debug(`setting features: ${JSON.stringify(features)}`);
        this.instance.clear();

        if (features.echoCancellation) {
            const model = features.echoCancellation === true ? 'full48k' : features.echoCancellation;
            this.instance.addDevererberator(WASM_PATH.dereverb[model]);
        }

        if (features.noiseCancellation) {
            const model = features.noiseCancellation === true ? 'full24k' : features.noiseCancellation;
            this.instance.addDenoiser(WASM_PATH.denoiser[model]);
        }

        if (features.normalization) {
            // TODO separate config for norm
            this.instance.addNormalizer(WASM_PATH.denoiser.full24k);
        }

        this.warmup();
        this.instance.resetContext();

        this.workletPort.postMessage({
            type: WORKLET_MESSAGES.WORKER_READY,
        });
        logger.debug('ready');
    }

    warmup() {
        for (let stepsCount = 0; stepsCount < WARMUP_STEP_COUNT; ++stepsCount) {
            this.instance.process(new Float32Array(SPENCH_FRAME_SIZE_SECONDS * this.sampleRate));
        }
    }

    handleWorkletMessage: WorkletPort['onmessage'] = (event) => {
        switch (event.data.type) {
            case WORKER_MESSAGES.INPUT:
                const { input } = event.data;
                const frameSize = Math.ceil(SPENCH_FRAME_SIZE_SECONDS * this.sampleRate);

                this.buffer.push(input);

                while (this.buffer.size >= frameSize) {
                    const frame = new Float32Array(frameSize);
                    this.buffer.pull(frame, 0, frameSize);

                    const startTime = performance.now();
                    const output = this.instance.process(frame);
                    const endTime = performance.now();

                    if (!output.length) {
                        return;
                    }

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
                telemetry.flush();

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

            const { wasm, data, workletPort, sampleRate, features } = event.data;
            runtime = new SpenchWorker(wasm, data, workletPort, sampleRate, features);

            return;
        case WORKER_MESSAGES.DESTROY:
            runtime?.destroy();
            runtime = null;

            logger.debug('destroyed');

            global.onmessage = null;
            global.close();

            return;
        case WORKER_MESSAGES.SET_FEATURES:
            runtime?.setFeatures(event.data.features);

            return;
        default:
    }

    assert(false, 'Unknown message type from Controller');
};

global.onmessage = handleControllerMessage;
