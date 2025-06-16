import {
    OVERFLOW_COUNTER_WINDOW_SECONDS,
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
import { Features } from '../../utils/common';

type ControllerPort = CustomPort<WorkletMessage, ControllerMessage>;
type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

// TODO shared array buffer
class SpenchWorkletProcessor extends AudioWorkletProcessor {
    readonly outputBuffer = new RingBuffer(Math.trunc(WORKLET_OUTPUT_BUFFER_SECONDS * sampleRate));

    readonly inputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));

    logger = getWorkerLogger(this.controllerPort, 'verbose', '[worklet]');

    telemetry = getWorkletTelemetry(false, this.controllerPort);

    instance: SpenchDspWrapper | undefined;

    workerPort: WorkerPort | undefined;

    isDspEnabled: boolean = false;

    isWorkerFeaturesEnabled: boolean = false;

    // State
    enabled = false;

    destroyed = false;

    workerInitialized = false;

    // Overflow
    previouslyRendered = false;

    overflowCounter = 0;

    overflowWindowStartTime: number | undefined;

    constructor() {
        super();

        this.controllerPort.addEventListener('message', this.handleControllerMessage);
        this.controllerPort.start();
    }

    get controllerPort(): ControllerPort {
        return super.port;
    }

    process(inputs: Float32Array[][], outputs: Float32Array[][], _parameters: Record<string, Float32Array>) {
        if (this.destroyed) {
            return false;
        }

        // Skip processing if not connected
        if (!inputs[0]?.[0]?.length || !outputs[0]?.[0]?.length) {
            return true;
        }

        if (!this.enabled || !(this.isDspEnabled || this.isWorkerFeaturesEnabled)) {
            this.pass(inputs, outputs);

            return true;
        }

        if (this.isWorkerFeaturesEnabled && this.workerInitialized) {
            this.processInput(inputs);
            this.processOutput(outputs);
        } else {
            this.pass(inputs, outputs);
        }

        if (this.isDspEnabled) {
            this.processDsp(outputs, outputs);
        }

        this.timerTick();

        return true;
    }

    processInput(inputs: Float32Array[][]): void {
        assert(this.workerPort, 'Worker must be ready to accept input');

        const [firstInput] = inputs;
        const [input] = firstInput;

        this.inputBuffer.push(input);
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

    processOutput(outputs: Float32Array[][]): void {
        const [output] = outputs;
        const [outputChannel] = output;
        const isBufferSufficientlyFilled = this.outputBuffer.size >= WORKLET_OUTPUT_BUFFER_THRESHOLD * sampleRate;
        const shouldRenderAnyway = this.outputBuffer.size >= outputChannel.length && this.previouslyRendered;

        if (isBufferSufficientlyFilled || shouldRenderAnyway) {
            this.outputBuffer.pull(outputChannel, 0, outputChannel.length);
            this.previouslyRendered = true;

            for (const restOutput of output) {
                if (restOutput !== outputChannel) {
                    restOutput.set(outputChannel);
                }
            }
        } else {
            this.handleOverflow();
        }
    }

    processDsp(inputs: Float32Array[][], outputs: Float32Array[][]) {
        assert(this.instance, 'dsp instance must be initialized before processing');
        const [input] = inputs;
        const [output] = outputs;
        const [inputChannel] = input;

        const result = this.instance.process(inputChannel);

        for (const outputChannel of output) {
            outputChannel.set(result);
        }
    }

    pass(inputs: Float32Array[][], outputs: Float32Array[][]) {
        const [input] = inputs;
        const [output] = outputs;
        const [inputChannel] = input;

        for (const outputChannel of output) {
            outputChannel.set(inputChannel);
        }
    }

    handleOverflow() {
        if (!this.previouslyRendered) {
            // The beginning of rendering
            return;
        }

        // This is frame skip and artifacts will appear
        this.overflowCounter++;

        if (typeof this.overflowWindowStartTime === 'undefined') {
            this.logger.warn('dropped frame detected, counting overflow');
            this.overflowWindowStartTime = currentTime;
        }
    }

    timerTick() {
        if (
            typeof this.overflowWindowStartTime === 'number' &&
            currentTime - this.overflowWindowStartTime >= OVERFLOW_COUNTER_WINDOW_SECONDS
        ) {
            this.controllerPort.postMessage({
                type: CONTROLLER_MESSAGES.OVERFLOW,
                overflowCount: this.overflowCounter,
            });

            this.overflowCounter = 0;
            this.overflowWindowStartTime = undefined;
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
                const { verbosity, isTelemetryEnabled } = event.data;

                this.logger = getWorkerLogger(this.controllerPort, verbosity, '[worklet]');
                this.telemetry = getWorkletTelemetry(isTelemetryEnabled, this.controllerPort);
                this.logger.debug('init data received');

                return;
            case WORKLET_MESSAGES.RUNTIME_ASSETS:
                const { dspWasm } = event.data;

                SpenchDspWrapper.createBridge(dspWasm);
                this.instance = new SpenchDspWrapper(sampleRate, this.logger);

                return;
            case WORKLET_MESSAGES.SET_FEATURES:
                const { features } = event.data;

                try {
                    this.setFeatures(features);
                } catch (e) {
                    this.controllerPort.postMessage({
                        type: CONTROLLER_MESSAGES.SET_FEATURES_FAIL,
                        error: e as Error,
                    });

                    return;
                }
                this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SET_FEATURES_OK });

                if (!this.isWorkerFeaturesEnabled) {
                    this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SYSTEM_READY });
                }

                return;
            case WORKLET_MESSAGES.WORKER_CREATED:
                const { workerPort } = event.data;
                this.workerPort = workerPort;
                this.workerPort.addEventListener('message', this.handleWorkerMessage);
                this.workerPort.start();

                return;
            case WORKLET_MESSAGES.ENABLE:
                this.enable();

                return;
            case WORKLET_MESSAGES.DISABLE:
                this.disable();

                return;
            case WORKLET_MESSAGES.DESTROY:
                this.destroy();

                return;
            case WORKLET_MESSAGES.WORKER_FEATURES_CHANGED:
                const { enabled } = event.data;
                this.isWorkerFeaturesEnabled = enabled;
                this.workerInitialized = false;
                this.previouslyRendered = false;
                this.overflowWindowStartTime = undefined;
                this.overflowCounter = 0;

                if (!enabled) {
                    this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SYSTEM_READY });
                }

                return;
            default:
        }

        assert(false, 'Unknown message type from Controller');
    });

    // eslint-disable-next-line @typescript-eslint/member-ordering
    handleWorkerMessage: Exclude<WorkerPort['onmessage'], null> = this.withErrorHandling((event) => {
        switch (event.data.type) {
            case WORKLET_MESSAGES.OUTPUT:
                if (!this.workerInitialized) {
                    return;
                }

                this.outputBuffer.push(event.data.output);
                this.telemetry.frameReceived();

                return;
            case WORKLET_MESSAGES.WORKER_READY:
                this.workerInitialized = true;
                this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SYSTEM_READY });

                if (this.isWorkerFeaturesEnabled) {
                    this.telemetry.start(currentTime);
                }

                return;
            default:
        }

        assert(false, 'Unknown message type from Worker');
    });

    setFeatures(features: Features) {
        assert(this.instance, 'spench dsp core instance must exist');
        this.logger.debug(`setting features: ${JSON.stringify(features)}`);
        this.instance.clear();
        this.isDspEnabled = [
            features.normalization,
            // features.echoCancellation
        ].some(Boolean);

        if (features.normalization) {
            this.instance.addNormalizer();
        }
    }

    enable() {
        if (this.enabled) return;

        this.enabled = true;

        this.previouslyRendered = false;
        this.overflowCounter = 0;
        this.overflowWindowStartTime = undefined;

        this.outputBuffer.clear();

        if (this.isWorkerFeaturesEnabled) {
            this.telemetry.start(currentTime);
        }

        this.resetContext();
        this.logger.debug('enabled');
    }

    disable() {
        if (!this.enabled) return;

        this.enabled = false;

        this.telemetry.stop(currentTime);
        this.logger.debug('disabled');
    }

    resetContext() {
        this.workerPort?.postMessage({ type: WORKER_MESSAGES.RESET });
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
    }
}

registerProcessor(SPENCH_WORKLET_NAME, SpenchWorkletProcessor);
