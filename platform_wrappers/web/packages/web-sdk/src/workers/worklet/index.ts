import {
    OVERFLOW_COUNTER_WINDOW_SECONDS,
    SPENCH_FRAME_SIZE_SECONDS,
    WARMUP_STEP_COUNT,
    WORKLET_OUTPUT_BUFFER_SECONDS,
    WORKLET_OUTPUT_BUFFER_THRESHOLD,
} from '../../utils/constants';
import RingBuffer from '../../utils/ringBuffer';
import { assert } from '../../utils/error';
import {
    CONTROLLER_MESSAGES,
    ControllerToWorkletMessage,
    WORKER_MESSAGES,
    WorkerToWorkletMessage,
    WORKLET_MESSAGES,
    WorkletToControllerMessage,
    WorkletToWorkerMessage,
} from '../../utils/ipc';
import { getWorkerLogger, Logger } from '../../utils/logger';
import { getWorkletTelemetry } from '../../utils/telemetry';
import { TypedMessageEvent } from '../../utils/common';

interface ControllerPort extends MessagePort {
    onmessage: ((this: ControllerPort, event: TypedMessageEvent<ControllerToWorkletMessage>) => void) | null;

    postMessage(message: WorkletToControllerMessage, transfer: Transferable[]): void;
    postMessage(message: WorkletToControllerMessage, options?: StructuredSerializeOptions): void;
}

interface WorkerPort extends MessagePort {
    onmessage: ((this: WorkerPort, event: TypedMessageEvent<WorkerToWorkletMessage>) => void) | null;

    postMessage(message: WorkletToWorkerMessage, transfer: Transferable[]): void;
    postMessage(message: WorkletToWorkerMessage, options?: StructuredSerializeOptions): void;
}

// TODO shared array buffer
// TODO unify with spench processor
class SpenchWorkletProcessor extends AudioWorkletProcessor {
    workerPort: WorkerPort | undefined;

    readonly logger: Logger;

    readonly outputBuffer = new RingBuffer(Math.trunc(WORKLET_OUTPUT_BUFFER_SECONDS * sampleRate));

    readonly telemetry = getWorkletTelemetry(this.controllerPort);

    // State
    enabled = false;

    destroyed = false;

    // Warmup
    initialized = false;

    warmupCallback = this.warmupAssert;

    // Overflow
    previouslyRendered = false;

    overflowCounter = 0;

    overflowWindowStartTime: number | undefined;

    constructor() {
        super();

        this.logger = getWorkerLogger(this.controllerPort, '[worklet]');
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
        if (!inputs[0]?.[0]?.length) {
            return true;
        }

        // Currently serves first channel of the first input only.
        // TODO unify with norm processor, use all available inputs & channels (why?)
        const [input] = inputs;
        const [output] = outputs;

        const [inputChannel] = input;

        if (!(this.enabled && this.initialized)) {
            for (const outputChannel of output) {
                outputChannel.set(inputChannel);
            }

            return true;
        }

        this.processInput(inputChannel);
        this.processOutput(output);

        this.timerTick();

        return true;
    }

    processInput(input: Float32Array): void {
        assert(this.workerPort, 'Worker must be ready to accept input');

        this.workerPort.postMessage(
            {
                type: WORKER_MESSAGES.INPUT,
                input,
            },
            [input.buffer],
        );
    }

    processOutput(output: Float32Array[]): void {
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
            default:
        }

        assert(false, 'Unknown message type from Controller');
    };

    handleWorkerMessage: WorkerPort['onmessage'] = (event) => {
        switch (event.data.type) {
            case WORKLET_MESSAGES.OUTPUT:
                this.outputBuffer.push(event.data.output);

                if (!this.initialized) {
                    this.warmupCallback();

                    return;
                }

                this.telemetry.frameReceived();

                return;
            case WORKLET_MESSAGES.WORKER_READY:
                this.warmup();

                return;
            default:
        }

        assert(false, 'Unknown message type from Worker');
    };

    async warmup() {
        if (this.initialized) {
            assert(false, 'Warmup already complete');
        }

        this.logger.debug('starting warmup');
        const times = []; // TODO startup metric
        const inputFrameSize = SPENCH_FRAME_SIZE_SECONDS * sampleRate;
        let stepsCount = 0;
        let startTime: number;
        this.previouslyRendered = true;

        this.warmupCallback = () => {
            if (stepsCount > 0) {
                this.processOutput([new Float32Array(inputFrameSize)]);
                times.push(currentTime - startTime);
            }

            if (stepsCount++ < WARMUP_STEP_COUNT) {
                startTime = currentTime;
                this.processInput(new Float32Array(inputFrameSize));

                return;
            }

            assert(this.outputBuffer.size === 0, 'Output buffer must be empty after warmup');

            // avoid duplicate logic
            this.enabled = true;
            this.disable();

            this.initialized = true;
            this.warmupCallback = this.warmupAssert;

            this.logger.debug('warmup finished');
            this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.SYSTEM_READY });
        };

        this.warmupCallback();
    }

    warmupAssert() {
        assert(false, 'Unexpected warmup callback');
    }

    enable() {
        if (this.enabled) return;

        this.enabled = true;
        this.outputBuffer.clear();
        this.telemetry.start(currentTime);
        this.logger.debug('enabled');
    }

    disable() {
        if (!this.enabled) return;

        this.enabled = false;
        this.previouslyRendered = false;

        this.outputBuffer.clear();
        this.telemetry.stop(currentTime);

        this.overflowCounter = 0;
        this.overflowWindowStartTime = undefined;

        this.resetContext();
        this.logger.debug('disabled');
    }

    resetContext() {
        this.workerPort?.postMessage({
            type: WORKER_MESSAGES.RESET,
        });
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

registerProcessor('spench-processor', SpenchWorkletProcessor);
