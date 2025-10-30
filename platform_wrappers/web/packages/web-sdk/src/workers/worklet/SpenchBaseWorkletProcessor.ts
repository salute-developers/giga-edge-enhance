import {
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
import { getSettingsWithDefaults, Settings } from '../../utils/common';

type ControllerPort = CustomPort<WorkletMessage, ControllerMessage>;
type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

export class SpenchBaseWorkletProcessor extends AudioWorkletProcessor {
    protected readonly outputBuffer = new RingBuffer(Math.trunc(WORKLET_OUTPUT_BUFFER_SECONDS * sampleRate));

    protected readonly inputBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));

    protected readonly loopbackBuffer = new RingBuffer(Math.trunc(2 * TRANSPORT_SIZE_SECONDS * sampleRate));

    protected logger = getWorkerLogger(this.controllerPort, 'verbose', '[worklet]');

    protected telemetry = getWorkletTelemetry(false, this.controllerPort);

    protected settings: Settings = getSettingsWithDefaults();

    protected workerPort: WorkerPort | undefined;

    protected areWorkerFeaturesEnabled = false;

    protected destroyed = false;

    // Overflow
    protected refillTimes: number[] = [];

    protected previouslyRendered = false;

    constructor() {
        super();

        this.controllerPort.addEventListener('message', this.handleControllerMessage);
        this.controllerPort.start();
    }

    protected get controllerPort(): ControllerPort {
        return super.port;
    }

    protected get isProcessingEnabled(): boolean {
        return this.areWorkerFeaturesEnabled;
    }

    process(inputs: Float32Array[][], outputs: Float32Array[][], _parameters: Record<string, Float32Array>) {
        try {
            this.timerTick();

            const isConnected = Boolean(inputs[0]?.[0]?.length && outputs[0]?.[0]?.length);

            if (!isConnected || this.destroyed || !this.isProcessingEnabled) {
                this.pass(inputs, outputs);

                return !this.destroyed;
            }

            const virtualOutputs = outputs.map((channels) =>
                channels.map((channel) => new Float32Array(channel.length)),
            );

            this.pass(inputs, virtualOutputs);
            this.processOnAudioThread(inputs, virtualOutputs);
            // This is false only when a frame skip happens and we don't want to render unprocessed output
            const shouldPass = this.processOnWorkerThread(inputs, virtualOutputs);

            if (shouldPass) {
                this.pass(virtualOutputs, outputs);
            }

            return true;
        } catch (e) {
            this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.ERROR, error: e as Error });

            return false;
        }
    }

    // Override in DSP worklet
    protected processOnAudioThread(_inputs: Float32Array[][], _outputs: Float32Array[][]): void {}

    protected processOnWorkerThread(inputs: Float32Array[][], outputs: Float32Array[][]): boolean {
        const [, loopback] = inputs;
        const [output] = outputs;
        const [outputChannel] = output;
        const [loopbackChannel = new Float32Array(outputChannel.length)] = loopback || [];

        if (this.areWorkerFeaturesEnabled) {
            this.processInput(outputChannel, loopbackChannel);

            return this.processOutput(outputs);
        }

        return true;
    }

    protected processInput(inputChannel: Float32Array, loopbackChannel: Float32Array): void {
        assert(this.workerPort, 'Worker must be ready to accept input');

        this.inputBuffer.push(inputChannel);
        this.loopbackBuffer.push(loopbackChannel);
        const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate);

        if (this.inputBuffer.size >= transferChunkSize) {
            const input = new Float32Array(transferChunkSize);
            const loopback = new Float32Array(transferChunkSize);
            this.inputBuffer.pull(input, 0, transferChunkSize);
            this.loopbackBuffer.pull(loopback, 0, transferChunkSize);

            this.workerPort.postMessage(
                {
                    type: WORKER_MESSAGES.INPUT,
                    input,
                    loopback,
                },
                [input.buffer, loopback.buffer],
            );
        }
    }

    protected processOutput(outputs: Float32Array[][]): boolean {
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

    protected pass(inputs: Float32Array[][], outputs: Float32Array[][]): void {
        const [input = []] = inputs;
        const [output = []] = outputs;
        const [inputChannel = new Float32Array()] = input;
        const [outputChannel = new Float32Array()] = output;

        if (inputChannel.length === outputChannel.length) {
            outputChannel.set(inputChannel);
        }
    }

    protected handleOverflow() {
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

    protected timerTick() {
        if (!this.telemetry.started && this.areWorkerFeaturesEnabled) {
            this.telemetry.start(currentTime);
        }
    }

    protected withErrorHandling = <T extends (...args: any[]) => any>(fn: T): T => {
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

    protected handleControllerMessageDerived(_event: MessageEvent<WorkletMessage>): boolean {
        return false;
    }

    // eslint-disable-next-line @typescript-eslint/member-ordering
    protected handleControllerMessage: Exclude<ControllerPort['onmessage'], null> = this.withErrorHandling((event) => {
        if (this.handleControllerMessageDerived(event)) {
            return;
        }

        switch (event.data.type) {
            case WORKLET_MESSAGES.INIT:
                const { verbosity, isTelemetryEnabled, settings } = event.data;

                this.logger = getWorkerLogger(this.controllerPort, verbosity, '[worklet]');
                this.telemetry = getWorkletTelemetry(isTelemetryEnabled, this.controllerPort);
                this.settings = settings;
                this.logger.debug('init data received');

                return;
            case WORKLET_MESSAGES.SET_FEATURES:
                const { workerFeaturesEnabled } = event.data;

                this.logger.debug(`Setting worklet state, worker features enabled: ${workerFeaturesEnabled}`);
                this.areWorkerFeaturesEnabled = workerFeaturesEnabled;

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
    protected handleWorkerMessage: Exclude<WorkerPort['onmessage'], null> = this.withErrorHandling((event) => {
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

    protected reset() {
        this.inputBuffer.clear();
        this.outputBuffer.clear();
        this.loopbackBuffer.clear();
        this.telemetry.stop(currentTime);
        this.refillTimes = [];

        this.previouslyRendered = false;
    }

    protected destroy() {
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
