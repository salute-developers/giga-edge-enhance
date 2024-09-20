import RingBuffer from '../../utils/ringBuffer';

type WorkerMessage = { type: 'output'; output: Float32Array };
type MainMessage = { type: 'worker_port'; port: MessagePort } | { type: 'reset' } | { type: 'delay' };

/** Every 384 samples for 24kHz is 16 ms.
 * So output jitter buffer is 160 ms.
 */
const OUTPUT_BUFFER_SIZE = 384 * 10;

class DenoiseWorkletProcessor extends AudioWorkletProcessor {
    workerPort: MessagePort | undefined;

    outputBuffer: RingBuffer;

    constructor() {
        super();
        this.outputBuffer = new RingBuffer(OUTPUT_BUFFER_SIZE);

        const onMessageFromWorker = (event: { data: WorkerMessage }) => {
            switch (event.data.type) {
                case 'output': {
                    this.outputBuffer.push_384(event.data.output);
                    break;
                }
                default: {
                    console.error('unknown type from WORKER');
                }
            }
        };

        const onMessageFromMain = (event: { data: MainMessage }) => {
            switch (event.data.type) {
                case 'worker_port': {
                    this.workerPort = event.data.port;
                    this.workerPort.onmessage = onMessageFromWorker;
                    break;
                }
                case 'reset': {
                    this.outputBuffer.clear();
                    break;
                }
                case 'delay': {
                    this.port.postMessage({
                        type: 'delay',
                        samples: this.outputBuffer.size,
                    });
                    break;
                }
                default: {
                    console.error('unknown type from MAIN');
                }
            }
        };

        this.port.onmessage = onMessageFromMain;
    }

    process(inputs: Float32Array[][], outputs: Float32Array[][], _parameters: Record<string, Float32Array>) {
        const input = inputs[0];
        const output = outputs[0];

        if (input.length === 0) {
            /** No input data to process. AudioContext should be suspended. */
            return true;
        }

        const firstInputChannel = input[0];
        this.workerPort?.postMessage({
            type: 'input',
            input: firstInputChannel,
        });

        const firstOutputChannel = output[0];

        if (this.outputBuffer.size >= 128) {
            this.outputBuffer.pull_128(firstOutputChannel);

            if (output.length >= 2) {
                /** Spench denoiser always uses 1 channel.
                 * So in case of stereo output the second channel is duplicated. */
                output[1].set(output[0]);
            }
        } else {
            this.port.postMessage({ type: 'no_output' });
        }

        return true;
    }
}

registerProcessor('spench-processor', DenoiseWorkletProcessor);
