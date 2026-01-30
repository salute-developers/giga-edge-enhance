/* eslint-disable no-restricted-globals */

import { serializeError } from 'serialize-error';

import {
    WORKER_MESSAGES,
    CONTROLLER_MESSAGES,
    SELF_WORKER_MESSAGES,
    CustomPort,
    WorkerMessage,
    WorkletMessage,
    SelfWorkerMessage,
    WorkerControllerMessage,
    ControllerMessage,
    WORKLET_MESSAGES,
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
import { SPENCH_FRAME_SIZE_SECONDS } from '../../utils/constants';

import Loader, { LoadError } from './loader';
import { InternalError, SpenchDspWrapper, SpenchWrapper } from './wrapper';
import { AbstractTransport, MessageTransport, Payload, SharedMemoryTransport } from './transport';

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

    protected readonly transport: AbstractTransport;

    protected mainFeaturesEnabled = false;

    protected dspFeaturesEnabled = false;

    constructor(
        protected readonly controllerPort: ControllerPort,
        protected readonly sampleRate: number,
        protected readonly workletPort: WorkletPort,
        buffer?: SharedArrayBuffer,
    ) {
        if (buffer) {
            this.transport = new SharedMemoryTransport(
                buffer!,
                sampleRate,
                SPENCH_FRAME_SIZE_SECONDS,
                this.handleFrame,
                this.onError,
            );
        } else {
            this.transport = new MessageTransport(workletPort, sampleRate, SPENCH_FRAME_SIZE_SECONDS, this.handleFrame);
        }
        this.controllerPort.addEventListener('message', this.handleControllerMessage);
        this.controllerPort.start();
    }

    destroy() {
        shared.telemetry.flush();
        this.instance?.destroy();
        this.dspInstance?.destroy();
        this.transport.close();
        this.controllerPort.close();
    }

    protected reset() {
        this.instance?.resetContext();
        this.dspInstance?.resetContext();
        this.transport.reset();
        shared.telemetry.flush();

        shared.logger.debug('context reset');
    }

    protected async setFeatures(features: Features<ModelsTmpl>): Promise<void> {
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

    protected async setMainFeatures(features: MainFeatures<ModelsTmpl>): Promise<void | LoadError | InternalError> {
        if (!this.instance) {
            if (!areMainFeaturesEnabled(features)) {
                return;
            }
            const instance = await SpenchWrapper.create({
                sampleRate: this.sampleRate,
                logger: shared.logger,
                loader: shared.loader!,
                onHeavyLoadStart: () => {
                    this.workletPort.postMessage({
                        type: WORKLET_MESSAGES.SET_FLAG,
                        isWorkerBlockExpected: true,
                    });
                },
                onHeavyLoadEnd: () => {
                    this.workletPort.postMessage({
                        type: WORKLET_MESSAGES.SET_FLAG,
                        isWorkerBlockExpected: false,
                    });
                },
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

    protected async setDspFeatures(features: DspFeatures): Promise<void | LoadError | InternalError> {
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

    protected process(input: Float32Array, loopback: Float32Array): void {
        let output = input;

        const startTime = performance.now();

        if (this.dspFeaturesEnabled) {
            assert(this.dspInstance, 'DSP instance must exist before processing');
            output = this.dspInstance.process(input, loopback);

            if (!output.length) {
                return;
            }
        }

        if (this.mainFeaturesEnabled) {
            assert(this.instance, 'Spench instance must exist before processing');
            output = this.instance.process(output);

            if (!output.length) {
                return;
            }
        }

        const elapsed = performance.now() - startTime;
        shared.telemetry.recordFrameProcessTime(elapsed);
        shared.telemetry.recordMeanFrameProcessTime(elapsed);

        this.transport.push(output);
    }

    protected readonly handleFrame = ({ input, loopback }: Payload) => {
        try {
            this.process(input, loopback);
        } catch (e) {
            this.onError(e as Error);
        }
    };

    protected readonly handleControllerMessage = (event: MessageEvent<WorkerMessage>) => {
        try {
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
        } catch (e) {
            this.onError(e as Error);
        }
    };

    protected readonly onError = (e: Error) => {
        this.controllerPort.postMessage({
            type: CONTROLLER_MESSAGES.ERROR,
            error: serializeError(e as Error),
        });
    };
}

export const handleControllerMessage = (event: MessageEvent<SelfWorkerMessage>) => {
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

                    const { sampleRate, workletPort, buffer } = e.data;
                    instances.add(new SpenchWorker(controllerPort, sampleRate, workletPort, buffer));
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
