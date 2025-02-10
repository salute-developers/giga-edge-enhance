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
import { WARMUP_STEP_COUNT, WASM_PATH } from '../../utils/constants';

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

class SpenchWorker {
    private readonly instance: SpenchWrapper;

    private readonly telemetry: WorkerTelemetry;

    constructor(
        private readonly controllerPort: ControllerPort,
        private readonly workletPort: WorkletPort,
        sampleRate: number,
        features: Features,
    ) {
        this.instance = new SpenchWrapper(sampleRate, logger);
        this.telemetry = getWorkerTelemetry(controllerPort);
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

        // if (features.echoCancellation) {
        //     const model = features.echoCancellation === true ? 'full48k' : features.echoCancellation;
        //     this.instance.addDevererberator(WASM_PATH.dereverb[model]);
        // }

        if (features.noiseCancellation) {
            // const model = features.noiseCancellation === true ? 'full24k' : features.noiseCancellation;
            const model = 'full24k';
            this.instance.addDenoiser(WASM_PATH.denoiser[model]);
        }

        if (features.normalization) {
            // TODO separate config for norm
            this.instance.addNormalizer(WASM_PATH.denoiser.full24k);
        }

        if (hasHeavyComputation) {
            this.warmup();
            this.instance.resetContext();
        }

        this.telemetry.enabled = hasHeavyComputation;

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

                const startTime = performance.now();
                const output = this.instance.process(input);
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

                this.telemetry.recordFrameProcessTime(endTime - startTime);

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

            const { wasm, data } = event.data;
            SpenchWrapper.createBridge(wasm, data);

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
