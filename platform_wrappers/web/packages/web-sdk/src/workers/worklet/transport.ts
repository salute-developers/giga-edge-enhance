import RingBuffer from '../../utils/ringBuffer';
import { CustomPort, WORKER_MESSAGES, WorkerMessage, WORKLET_MESSAGES, WorkletMessage } from '../../utils/ipc';
import { TRANSPORT_SIZE_SECONDS, WORKLET_OUTPUT_BUFFER_SECONDS } from '../../utils/constants';
import { WorkletTelemetry } from '../../utils/telemetry';
import { assert } from '../../utils/error';

export interface AbstractTransport {
    get outputBufferSize(): number;
    push(input: Float32Array, loopback: Float32Array): void;
    pull(output: Float32Array): void;
    reset(): void;
    close(): void;
}

type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

export class MessageTransport implements AbstractTransport {
    protected readonly outputBuffer = new RingBuffer(Math.ceil(WORKLET_OUTPUT_BUFFER_SECONDS * sampleRate));

    protected readonly inputBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));

    protected readonly loopbackBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));

    constructor(
        private readonly port: WorkerPort,
        private readonly telemetry: WorkletTelemetry,
        private readonly onError: (error: Error) => void,
    ) {
        this.port.addEventListener('message', this.handleWorkerMessage);
        this.port.start();
    }

    get outputBufferSize(): number {
        return this.outputBuffer.size;
    }

    push(inputChannel: Float32Array, loopbackChannel: Float32Array): void {
        this.inputBuffer.push(inputChannel);
        this.loopbackBuffer.push(loopbackChannel);
        const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate);

        if (this.inputBuffer.size >= transferChunkSize) {
            const input = new Float32Array(transferChunkSize);
            const loopback = new Float32Array(transferChunkSize);
            this.inputBuffer.pull(input, 0, transferChunkSize);
            this.loopbackBuffer.pull(loopback, 0, transferChunkSize);

            this.port.postMessage(
                {
                    type: WORKER_MESSAGES.INPUT,
                    input,
                    loopback,
                },
                [input.buffer, loopback.buffer],
            );
        }
    }

    pull(output: Float32Array): void {
        this.outputBuffer.pull(output, 0, output.length);
    }

    reset() {
        this.inputBuffer.clear();
        this.outputBuffer.clear();
        this.loopbackBuffer.clear();
    }

    close() {
        this.port.close();
    }

    protected readonly handleWorkerMessage = (event: MessageEvent<WorkletMessage>) => {
        try {
            switch (event.data.type) {
                case WORKLET_MESSAGES.OUTPUT:
                    this.outputBuffer.push(event.data.output);
                    this.telemetry.frameReceived();

                    return;
                default:
            }

            assert(false, 'Unknown message type from Worker');
        } catch (e) {
            this.onError(e as Error);
        }
    };
}
