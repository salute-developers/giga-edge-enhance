import { SPENCH_FRAME_SIZE_SECONDS } from '../../utils/constants';
import RingBuffer from '../../utils/ringBuffer';
import SpenchError, { assert } from '../../utils/error';
import { CONTROLLER_MESSAGES, WORKLET_MESSAGES, WorkletMessage } from '../../utils/ipc';
import { SpenchDspWorkletWrapper } from '../../utils/wrapper';
import { areDspFeaturesEnabled, DspFeatures } from '../../utils/common';

import { SpenchBaseWorkletProcessor } from './SpenchBaseWorkletProcessor';

class InternalError extends SpenchError {}

export class SpenchDspWorkletProcessor extends SpenchBaseWorkletProcessor {
    protected readonly dspInputBuffer = new RingBuffer(Math.trunc(2 * SPENCH_FRAME_SIZE_SECONDS * sampleRate));

    protected readonly dspLoopbackBuffer = new RingBuffer(Math.trunc(2 * SPENCH_FRAME_SIZE_SECONDS * sampleRate));

    protected readonly dspOutputBuffer = new RingBuffer(Math.trunc(2 * SPENCH_FRAME_SIZE_SECONDS * sampleRate));

    protected instance: SpenchDspWorkletWrapper | undefined;

    protected isDspEnabled: boolean = false;

    protected override get isProcessingEnabled(): boolean {
        return this.isDspEnabled || super.isProcessingEnabled;
    }

    protected override processOnAudioThread(inputs: Float32Array[][], outputs: Float32Array[][]): void {
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

    protected override timerTick() {
        super.timerTick();
        this.instance?.tick();
    }

    protected override handleControllerMessageDerived(event: MessageEvent<WorkletMessage>): boolean {
        switch (event.data.type) {
            case WORKLET_MESSAGES.SET_FEATURES:
                const { features, workerFeaturesEnabled, dspWasm } = event.data;
                const res = this.setFeatures(features!, workerFeaturesEnabled, dspWasm);

                this.controllerPort.postMessage({
                    type: CONTROLLER_MESSAGES.SET_FEATURES_RESULT,
                    errors: res instanceof InternalError ? [res] : [],
                    features:
                        res instanceof InternalError ? { normalization: false, echoCancellation: false } : features!,
                });

                return true;
            default:
        }

        return false;
    }

    protected setFeatures(
        features: DspFeatures,
        workerFeaturesEnabled: boolean,
        dspWasm?: ArrayBuffer,
    ): void | InternalError {
        this.logger.debug(
            `Setting features: ${JSON.stringify(features)}, worker features enabled: ${workerFeaturesEnabled}`,
        );
        this.areWorkerFeaturesEnabled = workerFeaturesEnabled;

        try {
            if (!this.instance) {
                this.instance = new SpenchDspWorkletWrapper(dspWasm!, sampleRate, this.logger);
                this.logger.debug('SpenchDSP instance created');
            }

            this.instance.clear();

            if (features.echoCancellation) {
                const { unstable_customAECConfig: customConfig } = this.settings;

                this.instance.addEchoCanceller(customConfig);
            }

            if (features.normalization) {
                const { unstable_customNormalizationConfig: customConfig } = this.settings;

                this.instance.addNormalizer(customConfig);
            }
        } catch (e) {
            this.isDspEnabled = false;

            return new InternalError(e);
        }

        this.isDspEnabled = areDspFeaturesEnabled(features);
    }

    protected override reset() {
        super.reset();
        this.instance?.resetContext();
        this.dspInputBuffer.clear();
        this.dspLoopbackBuffer.clear();
        this.dspOutputBuffer.clear();
    }

    protected override destroy() {
        super.destroy();

        this.instance?.destroy();
        this.instance = undefined;
    }
}
