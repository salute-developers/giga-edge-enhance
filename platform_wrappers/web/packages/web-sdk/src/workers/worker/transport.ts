import RingBuffer from '../../utils/ringBuffer';
import { CustomPort, WORKER_MESSAGES, WorkerMessage, WORKLET_MESSAGES, WorkletMessage } from '../../utils/ipc';
import { TRANSPORT_SIZE_SECONDS } from '../../utils/constants';
import { assert } from '../../utils/error';

export type Payload = {
    input: Float32Array;
    loopback: Float32Array;
};

export interface AbstractTransport {
    push(output: Float32Array): void;
    reset(): void;
    close(): void;
}

type WorkletPort = CustomPort<WorkerMessage, WorkletMessage>;

export class MessageTransport implements AbstractTransport {
    protected readonly outputBuffer;

    protected readonly inputBuffer;

    protected readonly loopbackBuffer;

    constructor(
        private readonly port: WorkletPort,
        private readonly sampleRate: number,
        private readonly frameSize: number,
        private readonly onFrameReceived: (payload: Payload) => void,
    ) {
        this.inputBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));
        this.loopbackBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));
        this.outputBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));
        this.port.addEventListener('message', this.handleWorkletMessage);
        this.port.start();
    }

    push(output: Float32Array): void {
        this.outputBuffer.push(output);
        const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * this.sampleRate);

        while (this.outputBuffer.size >= transferChunkSize) {
            const chunk = new Float32Array(transferChunkSize);
            this.outputBuffer.pull(chunk, 0, transferChunkSize);

            this.port.postMessage(
                {
                    type: WORKLET_MESSAGES.OUTPUT,
                    output: chunk,
                },
                [chunk.buffer],
            );
        }
    }

    reset() {
        this.inputBuffer.clear();
        this.outputBuffer.clear();
        this.loopbackBuffer.clear();
    }

    close() {
        this.port.close();
    }

    protected readonly handleWorkletMessage = (event: MessageEvent<WorkerMessage>) => {
        switch (event.data.type) {
            case WORKER_MESSAGES.INPUT:
                const { input, loopback } = event.data;
                const frameSize = this.frameSize * this.sampleRate;
                const inputBuffer = new Float32Array(frameSize);
                const loopbackBuffer = new Float32Array(frameSize);

                this.inputBuffer.push(input);
                this.loopbackBuffer.push(loopback);

                while (this.inputBuffer.size >= frameSize) {
                    this.inputBuffer.pull(inputBuffer, 0, frameSize);
                    this.loopbackBuffer.pull(loopbackBuffer, 0, frameSize);

                    this.onFrameReceived({
                        input: inputBuffer,
                        loopback: loopbackBuffer,
                    });
                }

                return;
            default:
        }

        assert(false, 'Unknown message type from Worklet');
    };
}
