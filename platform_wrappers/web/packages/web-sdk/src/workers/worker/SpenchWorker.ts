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
import SpenchError, { assert } from '../../utils/error';
import { SpenchDspWrapper, SpenchWrapper } from '../../utils/wrapper';
import { getWorkerLogger, Logger } from '../../utils/logger';
import { getWorkerTelemetry, WorkerTelemetry } from '../../utils/telemetry';
import {
    areDspFeaturesEnabled,
    areMainFeaturesEnabled,
    DspFeatures,
    Features,
    MainFeatures,
    Settings,
} from '../../utils/common';
import { SPENCH_FRAME_SIZE_SECONDS, TRANSPORT_SIZE_SECONDS, WASM_PATH } from '../../utils/constants';
import RingBuffer from '../../utils/ringBuffer';

import Loader, { LoadError } from './loader';

export class InternalError extends SpenchError {}

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

export const global = self as SpenchWorkerGlobalScope;
export const instances = new Set<SpenchWorker>();

export const shared: { logger: Logger; telemetry: WorkerTelemetry; loader?: Loader; settings?: Settings } = {
    logger: getWorkerLogger(global, 'verbose', '[worker]'),
    telemetry: getWorkerTelemetry(false, global),
};

export class SpenchWorker {
    protected instance: SpenchWrapper | undefined;

    protected dspInstance: SpenchDspWrapper | undefined;

    protected readonly inputBuffer: RingBuffer;

    protected readonly loopbackBuffer: RingBuffer;

    protected readonly outputBuffer: RingBuffer;

    protected mainFeaturesEnabled = false;

    protected dspFeaturesEnabled = false;

    constructor(
        protected readonly controllerPort: ControllerPort,
        protected readonly workletPort: WorkletPort,
        protected readonly sampleRate: number,
    ) {
        this.inputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));
        this.loopbackBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));
        this.outputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));
        this.workletPort.addEventListener('message', this.handleWorkletMessage);
        this.controllerPort.addEventListener('message', this.handleControllerMessage);
        this.workletPort.start();
        this.controllerPort.start();
    }

    destroy() {
        shared.telemetry.flush();
        this.instance?.destroy();
        this.dspInstance?.destroy();
        this.workletPort.close();
        this.controllerPort.close();
    }

    reset() {
        this.instance?.resetContext();
        this.dspInstance?.resetContext();
        this.inputBuffer.clear();
        this.outputBuffer.clear();
        shared.telemetry.flush();

        shared.logger.debug('context reset');
    }

    async setFeatures(features: Features): Promise<void> {
        shared.logger.debug(`Setting features: ${JSON.stringify(features)}`);

        const [mainResult, dspResult] = await Promise.all([
            this.setMainFeatures(features),
            this.setDspFeatures(features),
        ]);
        const errors = [];
        const resultFeatures = { ...features };

        if (mainResult instanceof SpenchError) {
            errors.push(mainResult);
            resultFeatures.dereverberation = false;
            resultFeatures.noiseCancellation = false;
        }

        if (dspResult instanceof SpenchError) {
            errors.push(dspResult);
            resultFeatures.noiseCancellation = false;
            resultFeatures.normalization = false;
        }

        this.controllerPort.postMessage({
            type: CONTROLLER_MESSAGES.SET_FEATURES_RESULT,
            errors,
            features: resultFeatures,
        });
    }

    async setMainFeatures(features: MainFeatures): Promise<void | LoadError | InternalError> {
        if (!this.instance) {
            if (!areMainFeaturesEnabled(features)) {
                return;
            }

            const [wasm, data] = await Promise.all([shared.loader!.loadWasm(), shared.loader!.loadModelsData()]);

            if (wasm instanceof LoadError) {
                return wasm;
            }

            if (data instanceof Error) {
                return data;
            }

            // don't need these anymore
            shared.loader!.dataLoadPromise = undefined;
            shared.loader!.wasmLoadPromise = undefined;

            try {
                this.instance = new SpenchWrapper(wasm, data, this.sampleRate, shared.logger);
            } catch (e) {
                return new InternalError(e);
            }
        }

        try {
            this.instance.clear();

            if (features.dereverberation) {
                const model = features.dereverberation === true ? 'full48k' : features.dereverberation;
                this.instance.addDereverberator(WASM_PATH.dereverb[model]);
            }

            if (features.noiseCancellation) {
                const model = features.noiseCancellation === true ? 'full24k' : features.noiseCancellation;
                this.instance.addDenoiser(WASM_PATH.denoiser[model]);
            }
        } catch (e) {
            this.mainFeaturesEnabled = false;

            return new InternalError(e);
        }

        this.mainFeaturesEnabled = areMainFeaturesEnabled(features);
    }

    async setDspFeatures(features: DspFeatures): Promise<void | LoadError | InternalError> {
        if (!this.dspInstance) {
            if (!areDspFeaturesEnabled(features)) {
                return;
            }

            const wasm = await shared.loader!.loadDSPWasm();

            if (wasm instanceof Error) {
                return wasm;
            }

            try {
                this.dspInstance = new SpenchDspWrapper(wasm, this.sampleRate, shared.logger);
            } catch (e) {
                return new InternalError(e);
            }
        }

        try {
            this.dspInstance.clear();

            if (features.echoCancellation) {
                const { unstable_customAECConfig: customConfig } = shared.settings!;

                this.dspInstance.addEchoCanceller(customConfig);
            }

            if (features.normalization) {
                const { unstable_customNormalizationConfig: customConfig } = shared.settings!;

                this.dspInstance.addNormalizer(customConfig);
            }
        } catch (e) {
            this.dspFeaturesEnabled = false;

            return new InternalError(e);
        }

        this.dspFeaturesEnabled = areDspFeaturesEnabled(features);
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

    process(): void {
        const frameSize = Math.ceil(SPENCH_FRAME_SIZE_SECONDS * this.sampleRate);

        while (this.inputBuffer.size >= frameSize) {
            const frame = new Float32Array(frameSize);
            const lbFrame = new Float32Array(frameSize);
            let output = frame;
            this.inputBuffer.pull(frame, 0, frameSize);
            this.loopbackBuffer.pull(lbFrame, 0, frameSize);

            const startTime = performance.now();

            if (this.dspFeaturesEnabled) {
                assert(this.dspInstance, 'DSP instance must exist before processing');
                output = this.dspInstance.process(frame, lbFrame);

                if (!output.length) {
                    continue;
                }
            }

            if (this.mainFeaturesEnabled) {
                assert(this.instance, 'Spench instance must exist before processing');
                output = this.instance.process(output);

                if (!output.length) {
                    continue;
                }
            }

            const elapsed = performance.now() - startTime;
            shared.telemetry.recordFrameProcessTime(elapsed);
            shared.telemetry.recordMeanFrameProcessTime(elapsed);

            this.outputBuffer.push(output);
            const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * this.sampleRate);

            if (this.outputBuffer.size >= transferChunkSize) {
                const chunk = new Float32Array(transferChunkSize);
                this.outputBuffer.pull(chunk, 0, transferChunkSize);

                this.workletPort.postMessage(
                    {
                        type: WORKLET_MESSAGES.OUTPUT,
                        output: chunk,
                    },
                    [chunk.buffer],
                );
            }
        }
    }

    // eslint-disable-next-line @typescript-eslint/member-ordering
    handleWorkletMessage: Exclude<WorkletPort['onmessage'], null> = this.withErrorHandling((event) => {
        switch (event.data.type) {
            case WORKER_MESSAGES.INPUT:
                const { input, loopback } = event.data;

                this.inputBuffer.push(input);
                this.loopbackBuffer.push(loopback);
                this.process();

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
                this.setFeatures(features);

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

export const handleControllerMessage: Exclude<SpenchWorkerGlobalScope['onmessage'], null> = (event) => {
    switch (event.data.type) {
        case SELF_WORKER_MESSAGES.INIT:
            const { verbosity, isTelemetryEnabled, resources, settings: rawSettings } = event.data;

            shared.logger = getWorkerLogger(global, verbosity, '[worker]');
            shared.telemetry = getWorkerTelemetry(isTelemetryEnabled, global);
            shared.settings = rawSettings;
            shared.loader = new Loader(resources, shared.telemetry, shared.settings);
            shared.logger.debug('Init data received');

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

            shared.logger.debug('destroyed');

            global.close();

            return;
        default:
    }

    assert(false, 'Unknown message type from Controller');
};
