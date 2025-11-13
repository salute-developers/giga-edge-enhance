/* eslint-disable no-restricted-globals */

import { serializeError } from 'serialize-error';

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
import { getWorkerLogger, Logger } from '../../utils/logger';
import { getWorkerTelemetry, WorkerTelemetry } from '../../utils/telemetry';
import {
    areDspFeaturesEnabled,
    areMainFeaturesEnabled,
    DspFeatures,
    Features,
    MainFeatures,
    ModelsTmpl,
    Settings,
} from '../../utils/common';
import { SPENCH_FRAME_SIZE_SECONDS, TRANSPORT_SIZE_SECONDS } from '../../utils/constants';
import RingBuffer from '../../utils/ringBuffer';

import Loader, { LoadError } from './loader';
import { InternalError, SpenchDspWrapper, SpenchWrapper } from './wrapper';

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

    async setFeatures(features: Features<ModelsTmpl>): Promise<void> {
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
            errors: errors.map((err) => serializeError(err)),
            features: resultFeatures,
        });
    }

    async setMainFeatures(features: MainFeatures<ModelsTmpl>): Promise<void | LoadError | InternalError> {
        if (!this.instance) {
            if (!areMainFeaturesEnabled(features)) {
                return;
            }
            const instance = await SpenchWrapper.create({
                sampleRate: this.sampleRate,
                logger: shared.logger,
                loader: shared.loader!,
            });

            if (instance instanceof SpenchError) {
                return instance;
            }

            this.instance = instance;
        }

        this.instance.clear();

        if (features.dereverberation) {
            assert(typeof features.dereverberation === 'string', 'dereverberation model type must be specified');
            const result = await this.instance.addDereverberator(features.dereverberation);

            if (result instanceof SpenchError) {
                return result;
            }
        }

        if (features.noiseCancellation) {
            shared.logger.debug('Setting noiseCancellation');
            assert(typeof features.noiseCancellation === 'string', 'noise cancellation model type must be specified');
            const result = await this.instance.addNoiseCanceller(features.noiseCancellation);

            if (result instanceof SpenchError) {
                return result;
            }
        }

        this.mainFeaturesEnabled = areMainFeaturesEnabled(features);
    }

    async setDspFeatures(features: DspFeatures): Promise<void | LoadError | InternalError> {
        if (!this.dspInstance) {
            if (!areDspFeaturesEnabled(features)) {
                return;
            }

            const instance = await SpenchDspWrapper.create({
                sampleRate: this.sampleRate,
                logger: shared.logger,
                loader: shared.loader!,
            });

            if (instance instanceof SpenchError) {
                return instance;
            }

            this.dspInstance = instance;
        }

        this.dspInstance.clear();

        if (features.echoCancellation) {
            const { unstable_customAECConfig: customConfig } = shared.settings!;

            const result = this.dspInstance.addEchoCanceller(customConfig);

            if (result instanceof SpenchError) {
                return result;
            }
        }

        if (features.normalization) {
            const { unstable_customNormalizationConfig: customConfig } = shared.settings!;

            const result = this.dspInstance.addNormalizer(customConfig);

            if (result instanceof SpenchError) {
                return result;
            }
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
                origThis.controllerPort.postMessage({
                    type: CONTROLLER_MESSAGES.ERROR,
                    error: serializeError(e as Error),
                });
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
