import { serializeError } from 'serialize-error';

import { WORKLET_OUTPUT_BUFFER_MIN_LATENCY, MAX_WORKER_BLOCK_TIME_SECONDS } from '../../utils/constants';
import { assert } from '../../utils/error';
import {
    CONTROLLER_MESSAGES,
    ControllerMessage,
    CustomPort,
    WorkerMessage,
    WORKLET_MESSAGES,
    WorkletMessage,
} from '../../utils/ipc';
import { getWorkerLogger } from '../../utils/logger';
import { getWorkletTelemetry } from '../../utils/telemetry';
import { getSettingsWithDefaults, Settings } from '../../utils/common';

import { AbstractTransport, MessageTransport, SharedMemoryTransport } from './transport';

type ControllerPort = CustomPort<WorkletMessage, ControllerMessage>;
type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

export class GigaDenoiserBaseWorkletProcessor extends AudioWorkletProcessor {
    protected transport: AbstractTransport | undefined;

    protected logger = getWorkerLogger(this.controllerPort, 'verbose', '[worklet]');

    protected telemetry = getWorkletTelemetry(false, this.controllerPort);

    protected settings: Settings = getSettingsWithDefaults();

    protected workerPort: WorkerPort | undefined;

    protected areWorkerFeaturesEnabled = false;

    protected destroyed = false;

    // Overflow
    protected refillTimes: number[] = [];

    protected workerBlockFactor = 0;

    protected refillStartTime = Infinity;

    protected previouslyRendered = false;

    protected currentBufferThreshold = WORKLET_OUTPUT_BUFFER_MIN_LATENCY;

    constructor() {
        super();

        this.controllerPort.addEventListener('message', this.handleControllerMessage);
        this.controllerPort.start();
    }

    protected get controllerPort(): ControllerPort {
        return this.port;
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
            this.onError(e as Error);

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
        assert(this.transport, 'transport must be initialized before using');

        this.transport.push(inputChannel, loopbackChannel);
    }

    protected processOutput(outputs: Float32Array[][]): boolean {
        assert(this.transport, 'transport must be initialized before using');

        const [output] = outputs;
        const [outputChannel] = output;
        const { outputBufferSize } = this.transport;
        const isBufferSufficientlyFilled = outputBufferSize >= this.currentBufferThreshold * sampleRate;
        const shouldRenderAnyway = outputBufferSize >= outputChannel.length && this.previouslyRendered;

        if (isBufferSufficientlyFilled || shouldRenderAnyway) {
            this.transport.pull(outputChannel);
            this.previouslyRendered = true;
            this.refillStartTime = Infinity;
        } else if (this.previouslyRendered) {
            this.logger.debug('The output buffer is empty, refilling');
            this.handleOutputOverflow();

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

    protected handleOutputOverflow() {
        this.previouslyRendered = false;
        this.refillStartTime = currentFrame / sampleRate;

        if (this.workerBlockFactor) {
            this.logger.debug('Skipping the overflow check as it is expected worker block time');

            return;
        }

        this.refillTimes = [
            ...this.refillTimes.filter((t) => currentTime - t <= this.settings.lowPerformanceWindowSeconds),
            currentTime,
        ];
        this.currentBufferThreshold =
            Math.max(this.refillTimes.length - 1, 0) * this.settings.latencyIncreaseStep + this.settings.minLatency;
        this.transport!.outputBufferMaxSizeSeconds = this.currentBufferThreshold;
        this.logger.debug(`Setting output buffer size to ${this.currentBufferThreshold} seconds`);

        if (this.refillTimes.length >= this.settings.lowPerformanceMaxRefillTimes) {
            this.logger.debug(
                `The output buffer is empty for ${this.settings.lowPerformanceMaxRefillTimes} times in ${this.settings.lowPerformanceWindowSeconds} seconds, emitting LOW_PERFORMANCE`,
            );
            this.refillTimes = [];
            this.refillStartTime = Infinity;
            this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.LOW_PERFORMANCE });
        }
    }

    protected timerTick() {
        if (!this.telemetry.started && this.areWorkerFeaturesEnabled) {
            this.telemetry.start(currentTime);
        }

        if (this.refillStartTime !== Infinity) {
            const delta = currentFrame / sampleRate - this.refillStartTime;

            if (this.workerBlockFactor && delta > MAX_WORKER_BLOCK_TIME_SECONDS) {
                this.logger.debug(
                    `Refill takes ${delta}s (max ${MAX_WORKER_BLOCK_TIME_SECONDS}s), while the worker being blocked. Emitting LOW_PERFORMANCE`,
                );
                this.refillStartTime = Infinity;
                this.controllerPort.postMessage({ type: CONTROLLER_MESSAGES.LOW_PERFORMANCE });

                return;
            }

            const maxRefillTime = this.settings.lowPerformanceMaxRefillTimeFactor * this.currentBufferThreshold;

            if (!this.workerBlockFactor && delta > maxRefillTime) {
                this.logger.debug(
                    `Refill takes ${delta}s (current max ${maxRefillTime}s). Trying to continue playback.`,
                );
                this.previouslyRendered = true;
            }
        }
    }

    protected readonly onError = (e: Error) => {
        this.controllerPort.postMessage({
            type: CONTROLLER_MESSAGES.ERROR,
            error: serializeError(e as Error),
        });
    };

    protected handleControllerMessageDerived(_event: MessageEvent<WorkletMessage>): boolean {
        return false;
    }

    protected onInit(): void {}

    protected readonly handleControllerMessage = (event: MessageEvent<WorkletMessage>) => {
        try {
            if (this.handleControllerMessageDerived(event)) {
                return;
            }

            switch (event.data.type) {
                case WORKLET_MESSAGES.INIT:
                    const { verbosity, isTelemetryEnabled, settings } = event.data;

                    this.logger = getWorkerLogger(this.controllerPort, verbosity, '[worklet]');
                    this.telemetry = getWorkletTelemetry(isTelemetryEnabled, this.controllerPort);
                    this.settings = settings;
                    this.currentBufferThreshold = settings.minLatency;
                    this.onInit();
                    this.logger.debug('init data received');

                    return;
                case WORKLET_MESSAGES.SET_FEATURES:
                    const { workerFeaturesEnabled } = event.data;

                    this.logger.debug(`Setting worklet state, worker features enabled: ${workerFeaturesEnabled}`);
                    this.areWorkerFeaturesEnabled = workerFeaturesEnabled;
                    this.refillTimes = [];

                    return;
                case WORKLET_MESSAGES.WORKER_CREATED:
                    const { workerPort, buffer } = event.data;

                    this.workerPort = workerPort;
                    this.workerPort.addEventListener('message', this.handleWorkerMessage);

                    if (buffer) {
                        this.transport = new SharedMemoryTransport(buffer, this.settings);
                    } else {
                        this.transport = new MessageTransport(workerPort, this.settings, this.telemetry, this.onError);
                    }

                    this.workerPort.start();
                    this.transport.outputBufferMaxSizeSeconds = this.currentBufferThreshold;

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
        } catch (e) {
            this.onError(e as Error);
        }
    };

    protected readonly handleWorkerMessage = (event: MessageEvent<WorkletMessage>) => {
        try {
            switch (event.data.type) {
                case WORKLET_MESSAGES.SET_FLAG:
                    const { isWorkerBlockExpected } = event.data;

                    if (typeof isWorkerBlockExpected === 'boolean') {
                        this.workerBlockFactor += isWorkerBlockExpected ? 1 : -1;

                        if (!this.workerBlockFactor && this.refillStartTime !== Infinity) {
                            this.refillStartTime = currentFrame / sampleRate;
                            this.refillTimes = [];
                        }
                    }

                    // eslint-disable-next-line no-useless-return
                    return;
                default:
            }
        } catch (e) {
            this.onError(e as Error);
        }
    };

    protected reset() {
        this.transport?.reset();
        this.telemetry.stop(currentTime);
        this.refillTimes = [];
        this.refillStartTime = Infinity;

        this.previouslyRendered = false;
    }

    protected destroy() {
        this.logger.debug('destroyed');

        this.destroyed = true;
        this.telemetry.stop(currentTime);
        this.controllerPort.close();

        this.workerPort?.close();
        this.transport = undefined;
    }
}
