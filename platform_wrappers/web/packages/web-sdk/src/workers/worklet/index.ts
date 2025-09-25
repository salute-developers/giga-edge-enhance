import {
    SPENCH_FRAME_SIZE_SECONDS,
    SPENCH_WORKLET_NAME,
    TRANSPORT_SIZE_SECONDS,
    WORKLET_OUTPUT_BUFFER_SECONDS,
    WORKLET_OUTPUT_BUFFER_THRESHOLD,
} from '../../utils/constants';
import RingBuffer from '../../utils/ringBuffer';
import { assert } from '../../utils/error';
import {
    CONTROLLER_MESSAGES,
    ControllerMessage,
    CustomPort,
    WORKER_MESSAGES,
    WorkerMessage,
    WORKLET_MESSAGES,
    WorkletMessage,
} from '../../utils/ipc';
import { getWorkerLogger } from '../../utils/logger';
import { getWorkletTelemetry } from '../../utils/telemetry';
import { SpenchDspWrapper } from '../../utils/wrapper';
import { getSettingsWithDefaults, Settings, WorkletFeatures } from '../../utils/common';

type ControllerPort = CustomPort<WorkletMessage, ControllerMessage>;
type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

// TODO shared array buffer
class SpenchWorkletProcessor extends AudioWorkletProcessor {
    readonly outputBuffer = new RingBuffer(Math.trunc(WORKLET_OUTPUT_BUFFER_SECONDS * sampleRate));

    readonly inputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));

    readonly dspInputBuffer = new RingBuffer(Math.trunc(2 * SPENCH_FRAME_SIZE_SECONDS * sampleRate));

    readonly dspLoopbackBuffer = new RingBuffer(Math.trunc(2 * SPENCH_FRAME_SIZE_SECONDS * sampleRate));

    readonly dspOutputBuffer = new RingBuffer(Math.trunc(2 * SPENCH_FRAME_SIZE_SECONDS * sampleRate));

    logger = getWorkerLogger(this.controllerPort, 'verbose', '[worklet]');

    telemetry = getWorkletTelemetry(false, this.controllerPort);

    instance: SpenchDspWrapper | undefined;

    settings: Settings = getSettingsWithDefaults();

    workerPort: WorkerPort | undefined;

    isDspEnabled: boolean = false;

    areWorkerFeaturesEnabled = false;

    destroyed = false;

    // Overflow
    refillTimes: number[] = [];

    previouslyRendered = false;

    constructor() {
        super();

        this.controllerPort.addEventListener('message', this.handleControllerMessage);
        this.controllerPort.start();
    }

    get controllerPort(): ControllerPort {
        return super.port;
    }

    process(inputs: Float32Array[][], outputs: Float32Array[][], _parameters: Record<string, Float32Array>) {
        // Skip processing if not connected
        if (!inputs[0]?.[0]?.length || !outputs[0]?.[0]?.length) {
            return !this.destroyed;
        }

        const processingEnabled = this.isDspEnabled || this.areWorkerFeaturesEnabled;

        if (this.destroyed || !processingEnabled) {
            this.pass(inputs, outputs);

            return !this.destroyed;
        }

        this.timerTick();

        const virtualOutputs = outputs.map((channels) => channels.map((channel) => new Float32Array(channel.length)));

        if (this.isDspEnabled) {
            this.processDsp(inputs, virtualOutputs);
        } else {
            this.pass(inputs, virtualOutputs);
        }

        if (this.areWorkerFeaturesEnabled) {
            this.processInput(virtualOutputs);

            if (!this.processOutput(virtualOutputs)) {
                return true;
            }
        }

        this.pass(virtualOutputs, outputs);

        return true;
    }

    processInput(inputs: Float32Array[][]): void {
        assert(this.workerPort, 'Worker must be ready to accept input');

        const [input] = inputs;
        const [inputChannel] = input;

        this.inputBuffer.push(inputChannel);
        const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate);
        const chunksToSend = Math.trunc(this.inputBuffer.size / transferChunkSize);

        if (chunksToSend > 0) {
            const chunk = new Float32Array(transferChunkSize * chunksToSend);
            this.inputBuffer.pull(chunk, 0, transferChunkSize * chunksToSend);

            this.workerPort.postMessage(
                {
                    type: WORKER_MESSAGES.INPUT,
                    input: chunk,
                },
                [chunk.buffer],
            );
        }
    }

    processOutput(outputs: Float32Array[][]): boolean {
        const [output] = outputs;
        const [outputChannel] = output;
        const isBufferSufficientlyFilled = this.outputBuffer.size >= WORKLET_OUTPUT_BUFFER_THRESHOLD * sampleRate;
        const shouldRenderAnyway = this.outputBuffer.size >= outputChannel.length && this.previouslyRendered;

        if (isBufferSufficientlyFilled || shouldRenderAnyway) {
            this.outputBuffer.pull(outputChannel, 0, outputChannel.length);
            this.previouslyRendered = true;
        } else if (this.previouslyRendered) {
            // This is frame skip and artifacts will appear
            this.handleOverflow();

            return false;
        }

        return true;
    }

    processDsp(inputs: Float32Array[][], outputs: Float32Array[][]) {
        assert(this.instance, 'dsp instance must be initialized before processing');
        const frameSize = Math.ceil(SPENCH_FRAME_SIZE_SECONDS * sampleRate);
        const [input, loopback] = inputs;
        const [output] = outputs;
        const [inputChannel] = input;
        const [outputChannel] = output;
        const [loopbackChannel = new Float32Array(inputChannel.length)] = loopback || [];

        this.dspInputBuffer.push(inputChannel);
        this.dspLoopbackBuffer.push(loopbackChannel);

        if (this.dspInputBuffer.size >= frameSize) {
            const inputFrame = new Float32Array(frameSize);
            const loopbackFrame = new Float32Array(frameSize);
            this.dspInputBuffer.pull(inputFrame, 0, frameSize);
            this.dspLoopbackBuffer.pull(loopbackFrame, 0, frameSize);

            this.dspOutputBuffer.push(this.instance.process(inputFrame, loopbackFrame));
        }

        if (this.dspOutputBuffer.size >= outputChannel.length) {
            this.dspOutputBuffer.pull(outputChannel, 0, outputChannel.length);
        }
    }

    pass(inputs: Float32Array[][], outputs: Float32Array[][]) {
        const [input] = inputs;
        const [output] = outputs;
        const [inputChannel] = input;
        const [outputChannel] = output;

        outputChannel.set(inputChannel);
    }

    handleOverflow() {
        this.logger.debug('The output buffer is empty, refilling');

        this.refillTimes = [
            ...this.refillTimes.filter((t) => currentTime - t <= this.settings.lowPerformanceWindowSeconds),
            currentTime,
        ];
        this.previouslyRendered = false;

        if (this.refillTimes.length >= this.settings.lowPerformanceMaxRefillTimes) {
            this.logger.debug(
                `The output buffer is empty for ${this.settings.lowPerformanceMaxRefillTimes} times in ${this.settings.lowPerformanceWindowSeconds} seconds, emitting LOW_PERFORMANCE`,
            );
            this.refillTimes = [];
            this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.LOW_PERFORMANCE });
        }
    }

    timerTick() {
        this.instance?.tick();

        if (!this.telemetry.started && this.areWorkerFeaturesEnabled) {
            this.telemetry.start(currentTime);
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
    handleControllerMessage: Exclude<ControllerPort['onmessage'], null> = this.withErrorHandling((event) => {
        switch (event.data.type) {
            case WORKLET_MESSAGES.INIT:
                const { verbosity, isTelemetryEnabled, settings } = event.data;

                this.logger = getWorkerLogger(this.controllerPort, verbosity, '[worklet]');
                this.telemetry = getWorkletTelemetry(isTelemetryEnabled, this.controllerPort);
                this.settings = settings;
                this.logger.debug('init data received');

                return;
            case WORKLET_MESSAGES.RUNTIME_ASSETS:
                const { dspWasm } = event.data;

                this.instance = new SpenchDspWrapper(dspWasm, sampleRate, this.logger);
                this.logger.debug('SpenchDSP instance created');

                return;
            case WORKLET_MESSAGES.SET_FEATURES:
                const { features, workerFeaturesEnabled } = event.data;

                try {
                    this.setFeatures(features, workerFeaturesEnabled);
                } catch (e) {
                    this.controllerPort.postMessage({
                        type: CONTROLLER_MESSAGES.SET_FEATURES_FAIL,
                        error: e as Error,
                    });

                    return;
                }
                this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SET_FEATURES_OK });

                return;
            case WORKLET_MESSAGES.WORKER_CREATED:
                const { workerPort } = event.data;
                this.workerPort = workerPort;
                this.workerPort.addEventListener('message', this.handleWorkerMessage);
                this.workerPort.start();

                return;
            case WORKLET_MESSAGES.RESET:
                this.reset();

                return;
            case WORKLET_MESSAGES.DESTROY:
                this.destroy();

                return;
            default:
        }

        assert(false, 'Unknown message type from Controller');
    });

    // eslint-disable-next-line @typescript-eslint/member-ordering
    handleWorkerMessage: Exclude<WorkerPort['onmessage'], null> = this.withErrorHandling((event) => {
        switch (event.data.type) {
            case WORKLET_MESSAGES.OUTPUT:
                if (!this.areWorkerFeaturesEnabled) {
                    return;
                }

                this.outputBuffer.push(event.data.output);
                this.telemetry.frameReceived();

                return;
            default:
        }

        assert(false, 'Unknown message type from Worker');
    });

    setFeatures(features: WorkletFeatures, workerFeaturesEnabled: boolean) {
        this.logger.debug(
            `Setting features: ${JSON.stringify(features)}, worker features enabled: ${workerFeaturesEnabled}`,
        );

        this.isDspEnabled = [features.normalization, features.echoCancellation].some(Boolean);
        this.areWorkerFeaturesEnabled = workerFeaturesEnabled;

        if (this.isDspEnabled) {
            assert(this.instance, 'spench dsp core instance must exist');
            this.instance.clear();

            if (features.echoCancellation) {
                const { unstable_customAECConfig: customConfig } = this.settings;

                this.instance.addEchoCanceller(customConfig);
            }

            if (features.normalization) {
                const { unstable_customNormalizationConfig: customConfig } = this.settings;

                this.instance.addNormalizer(customConfig);
            }
        }
    }

    reset() {
        this.instance?.resetContext();
        this.inputBuffer.clear();
        this.outputBuffer.clear();
        this.telemetry.stop(currentTime);

        this.previouslyRendered = false;
    }

    destroy() {
        this.logger.debug('destroyed');

        this.destroyed = true;
        this.telemetry.stop(currentTime);
        this.controllerPort.close();

        if (this.workerPort) {
            this.workerPort.close();
            this.workerPort = undefined;
        }

        this.instance?.destroy();
        this.instance = undefined;
    }
}

registerProcessor(SPENCH_WORKLET_NAME, SpenchWorkletProcessor);
