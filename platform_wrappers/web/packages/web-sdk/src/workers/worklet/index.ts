import {
    OVERFLOW_COUNTER_WINDOW_SECONDS,
    SPENCH_WORKLET_NAME,
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
import { getWorkerLogger, Logger } from '../../utils/logger';
import { getWorkletTelemetry } from '../../utils/telemetry';

type ControllerPort = CustomPort<WorkletMessage, ControllerMessage>;
type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

// TODO shared array buffer
class SpenchWorkletProcessor extends AudioWorkletProcessor {
    workerPort: WorkerPort | undefined;

    readonly logger: Logger = getWorkerLogger(this.controllerPort, '[worklet]');

    readonly outputBuffer = new RingBuffer(Math.trunc(WORKLET_OUTPUT_BUFFER_SECONDS * sampleRate));

    readonly telemetry = getWorkletTelemetry(this.controllerPort);

    // State
    enabled = false;

    destroyed = false;

    initialized = false;

    // Overflow
    previouslyRendered = false;

    overflowCounter = 0;

    overflowWindowStartTime: number | undefined;

    constructor() {
        super();

        this.controllerPort.onmessage = this.handleControllerMessage;
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

        if (!this.enabled || !this.initialized) {
            this.pass(inputs, outputs);

            return true;
        }

        this.processInput(inputs);
        this.processOutput(outputs);

        this.timerTick();

        return true;
    }

    processInput(inputs: Float32Array[][]): void {
        assert(this.workerPort, 'Worker must be ready to accept input');

        const [firstInput] = inputs;
        const [input] = firstInput;

        this.workerPort.postMessage(
            {
                type: WORKER_MESSAGES.INPUT,
                input,
            },
            [input.buffer],
        );
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

    handleControllerMessage: ControllerPort['onmessage'] = (event) => {
        switch (event.data.type) {
            case WORKLET_MESSAGES.INIT:
                this.logger.debug('init data received');
                const { workerPort } = event.data;

                this.workerPort = workerPort;
                this.workerPort.onmessage = this.handleWorkerMessage;

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
            case WORKLET_MESSAGES.WORKER_EJECTED:
                this.initialized = false;

                return;
            default:
        }

        assert(false, 'Unknown message type from Controller');
    };

    handleWorkerMessage: WorkerPort['onmessage'] = (event) => {
        switch (event.data.type) {
            case WORKLET_MESSAGES.OUTPUT:
                if (this.initialized) {
                    this.outputBuffer.push(event.data.output);
                    this.telemetry.frameReceived();
                }

                return;
            case WORKLET_MESSAGES.WORKER_READY:
                this.initialized = true;
                this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SYSTEM_READY });

                return;
            default:
        }

        assert(false, 'Unknown message type from Worker');
    };

    enable() {
        if (this.enabled) return;

        this.enabled = true;

        this.previouslyRendered = false;
        this.overflowCounter = 0;
        this.overflowWindowStartTime = undefined;

        this.outputBuffer.clear();
        this.telemetry.start(currentTime);
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
        this.controllerPort.onmessage = null;

        if (this.workerPort) {
            this.workerPort.close();
            this.workerPort.onmessage = null;
            this.workerPort = undefined;
        }
    }
}

registerProcessor(SPENCH_WORKLET_NAME, SpenchWorkletProcessor);
