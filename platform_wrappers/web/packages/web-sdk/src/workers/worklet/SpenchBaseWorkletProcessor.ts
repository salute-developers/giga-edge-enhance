import { serializeError } from 'serialize-error';

import { WORKLET_OUTPUT_BUFFER_MIN_LATENCY, WORKLET_OUTPUT_BUFFER_SECONDS } from '../../utils/constants';
import { assert } from '../../utils/error';
import { CONTROLLER_MESSAGES, ControllerMessage, CustomPort, WORKLET_MESSAGES, WorkletMessage } from '../../utils/ipc';
import { getWorkerLogger } from '../../utils/logger';
import { getWorkletTelemetry } from '../../utils/telemetry';
import { getSettingsWithDefaults, Settings } from '../../utils/common';

import { AbstractTransport, MessageTransport } from './transport';

type ControllerPort = CustomPort<WorkletMessage, ControllerMessage>;

export class SpenchBaseWorkletProcessor extends AudioWorkletProcessor {
    protected transport: AbstractTransport | undefined;

    protected logger = getWorkerLogger(this.controllerPort, 'verbose', '[worklet]');

    protected telemetry = getWorkletTelemetry(false, this.controllerPort);

    protected settings: Settings = getSettingsWithDefaults();

    protected areWorkerFeaturesEnabled = false;

    protected destroyed = false;

    // Overflow
    protected refillTimes: number[] = [];

    protected previouslyRendered = false;

    protected currentBufferThreshold = WORKLET_OUTPUT_BUFFER_MIN_LATENCY;

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
        const isBufferSufficientlyFilled = this.transport.outputBufferSize >= this.currentBufferThreshold * sampleRate;
        const shouldRenderAnyway = this.transport.outputBufferSize >= outputChannel.length && this.previouslyRendered;

        if (isBufferSufficientlyFilled || shouldRenderAnyway) {
            this.transport.pull(outputChannel);
            this.previouslyRendered = true;
        } else if (this.previouslyRendered) {
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
        this.currentBufferThreshold = Math.min(
            (this.refillTimes.length - 1) * this.settings.latencyIncreaseStep + this.settings.minLatency,
            WORKLET_OUTPUT_BUFFER_SECONDS,
        );
        this.logger.debug(`Setting output buffer size to ${this.currentBufferThreshold} seconds`);

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

    protected readonly onError = (e: Error) => {
        this.controllerPort.postMessage({
            type: CONTROLLER_MESSAGES.ERROR,
            error: serializeError(e as Error),
        });
    };

    protected handleControllerMessageDerived(_event: MessageEvent<WorkletMessage>): boolean {
        return false;
    }

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
                    this.logger.debug('init data received');

                    return;
                case WORKLET_MESSAGES.SET_FEATURES:
                    const { workerFeaturesEnabled } = event.data;

                    this.logger.debug(`Setting worklet state, worker features enabled: ${workerFeaturesEnabled}`);
                    this.areWorkerFeaturesEnabled = workerFeaturesEnabled;
                    this.refillTimes = [];

                    return;
                case WORKLET_MESSAGES.WORKER_CREATED:
                    const { workerPort } = event.data;
                    this.transport = new MessageTransport(workerPort, this.telemetry, this.onError);

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

    protected reset() {
        this.transport?.reset();
        this.telemetry.stop(currentTime);
        this.refillTimes = [];

        this.previouslyRendered = false;
    }

    protected destroy() {
        this.logger.debug('destroyed');

        this.destroyed = true;
        this.telemetry.stop(currentTime);
        this.controllerPort.close();

        this.transport?.close();
        this.transport = undefined;
    }
}
