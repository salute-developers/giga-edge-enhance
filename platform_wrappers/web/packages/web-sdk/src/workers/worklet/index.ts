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

    areWorkerFeaturesEnabled = false;

    // State
    enabled = false;

    destroyed = false;

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

        const processingEnabled = this.isDspEnabled || this.areWorkerFeaturesEnabled;

        if (!this.enabled || !processingEnabled) {
            this.pass(inputs, outputs);

            return true;
        }

        this.timerTick();

        const virtualOutputs = outputs.map((channels) => channels.map((channel) => new Float32Array(channel.length)));
        this.pass(inputs, virtualOutputs);

        if (this.isDspEnabled) {
            this.processDsp(virtualOutputs, virtualOutputs);
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

            for (const restOutput of output) {
                if (restOutput !== outputChannel) {
                    restOutput.set(outputChannel);
                }
            }
        } else if (this.previouslyRendered) {
            // This is frame skip and artifacts will appear
            this.handleOverflow();

            return false;
        }

        return true;
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
        this.overflowCounter++;

        if (typeof this.overflowWindowStartTime === 'undefined') {
            this.logger.warn('dropped frame detected, counting overflow');
            this.overflowWindowStartTime = currentTime;
        }
    }

    timerTick() {
        this.instance?.tick();

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

                SpenchDspWrapper.createBridge(dspWasm, this.logger.info);
                this.instance = new SpenchDspWrapper(sampleRate, this.logger);
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
            case WORKLET_MESSAGES.ENABLE:
                this.enable();

                return;
            case WORKLET_MESSAGES.DISABLE:
                this.disable();

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

    setFeatures(features: Features, workerFeaturesEnabled: boolean) {
        this.logger.debug(
            `Setting features: ${JSON.stringify(features)}, worker features enabled: ${workerFeaturesEnabled}`,
        );

        this.isDspEnabled = [
            features.normalization,
            // features.echoCancellation
        ].some(Boolean);
        this.areWorkerFeaturesEnabled = workerFeaturesEnabled;

        if (this.isDspEnabled) {
            assert(this.instance, 'spench dsp core instance must exist');
            this.instance.clear();

            if (features.normalization) {
                this.instance.addNormalizer();
            }
        }
    }

    reset() {
        this.instance?.resetContext();
        this.inputBuffer.clear();
        this.outputBuffer.clear();

        this.previouslyRendered = false;
        this.overflowCounter = 0;
        this.overflowWindowStartTime = undefined;

        if (this.areWorkerFeaturesEnabled) {
            this.telemetry.start(currentTime);
        }
    }

    enable() {
        this.enabled = true;

        this.reset();
        this.logger.debug('enabled');
    }

    disable() {
        this.enabled = false;

        this.telemetry.stop(currentTime);
        this.logger.debug('disabled');
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
    }
}

registerProcessor(SPENCH_WORKLET_NAME, SpenchWorkletProcessor);
