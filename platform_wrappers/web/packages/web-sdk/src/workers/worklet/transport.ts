import { RingBuffer } from '../../utils/ringBuffer';
import { CustomPort, WORKER_MESSAGES, WorkerMessage, WORKLET_MESSAGES, WorkletMessage } from '../../utils/ipc';
import {
    TRANSPORT_SIZE_SECONDS,
    OUTPUT_BUFFER_SECONDS,
    getInputBufferSize,
    getOutputBufferSize,
} from '../../utils/constants';
import { WorkletTelemetry } from '../../utils/telemetry';
import { BinarySemaphore } from '../../utils/semaphore';

export interface AbstractTransport {
    get outputBufferSize(): number;
    set outputBufferMaxSize(size: number);
    push(input: Float32Array, loopback: Float32Array): void;
    pull(output: Float32Array): boolean;
    reset(): void;
}

type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

export class MessageTransport implements AbstractTransport {
    protected readonly outputBuffer = new RingBuffer(Math.ceil(OUTPUT_BUFFER_SECONDS * sampleRate));

    protected readonly inputBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));

    protected readonly loopbackBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));

    constructor(
        protected readonly port: WorkerPort,
        protected readonly telemetry: WorkletTelemetry,
        protected readonly onError: (error: Error) => void,
    ) {
        this.port.addEventListener('message', this.handleWorkerMessage);
    }

    get outputBufferSize(): number {
        return this.outputBuffer.size;
    }

    set outputBufferMaxSize(value: number) {
        this.outputBuffer.maxSize = value;
    }

    push(inputChannel: Float32Array, loopbackChannel: Float32Array) {
        this.inputBuffer.push(inputChannel);
        this.loopbackBuffer.push(loopbackChannel);
        const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate);

        if (this.inputBuffer.size >= transferChunkSize) {
            const input = new Float32Array(transferChunkSize);
            const loopback = new Float32Array(transferChunkSize);
            this.inputBuffer.pull(input);
            this.loopbackBuffer.pull(loopback);

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

    pull(output: Float32Array): boolean {
        if (this.outputBuffer.size >= output.length) {
            this.outputBuffer.pull(output);

            return true;
        }

        return false;
    }

    reset() {
        this.inputBuffer.clear();
        this.outputBuffer.clear();
        this.loopbackBuffer.clear();
    }

    protected readonly handleWorkerMessage = (event: MessageEvent<WorkletMessage>) => {
        try {
            switch (event.data.type) {
                case WORKLET_MESSAGES.OUTPUT:
                    this.outputBuffer.push(event.data.output);
                    this.telemetry.frameReceived();

                    // eslint-disable-next-line no-useless-return
                    return;
                default:
            }
        } catch (e) {
            this.onError(e as Error);
        }
    };
}

export class SharedMemoryTransport implements AbstractTransport {
    protected readonly inputBuffer: RingBuffer;

    protected readonly loopbackBuffer: RingBuffer;

    protected readonly outputBuffer: RingBuffer;

    protected readonly semaphore: BinarySemaphore;

    constructor(buffer: SharedArrayBuffer) {
        let offset = 0;
        const inputSize = getInputBufferSize(sampleRate);
        const inputABSize = RingBuffer.getInternalBufferSize(inputSize);
        const outputSize = getOutputBufferSize(sampleRate);

        this.inputBuffer = new RingBuffer(inputSize, buffer);
        offset += inputABSize;

        this.loopbackBuffer = new RingBuffer(inputSize, buffer, offset);
        offset += inputABSize;

        this.outputBuffer = new RingBuffer(outputSize, buffer, offset);
        offset += RingBuffer.getInternalBufferSize(outputSize);

        this.semaphore = new BinarySemaphore(buffer, offset);
    }

    get outputBufferSize(): number {
        return this.outputBuffer.size;
    }

    set outputBufferMaxSize(value: number) {
        this.outputBuffer.maxSize = value;
    }

    push(inputChannel: Float32Array, loopbackChannel: Float32Array) {
        this.loopbackBuffer.push(loopbackChannel);
        this.inputBuffer.push(inputChannel);
    }

    pull(output: Float32Array): boolean {
        if (this.outputBuffer.size >= output.length) {
            this.outputBuffer.pull(output);

            return true;
        }

        return false;
    }

    reset() {
        this.semaphore.lock();

        this.inputBuffer.clear();
        this.loopbackBuffer.clear();

        this.semaphore.unlock();

        this.outputBuffer.clear();
    }
}
