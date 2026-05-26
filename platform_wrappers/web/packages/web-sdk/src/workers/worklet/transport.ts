import { RingBuffer } from '../../utils/ringBuffer';
import { CustomPort, WORKER_MESSAGES, WorkerMessage, WORKLET_MESSAGES, WorkletMessage } from '../../utils/ipc';
import { Settings } from '../../utils/common';
import {
    getBufferSize,
    getInputBufferSeconds,
    getOutputBufferSeconds,
    TRANSPORT_SIZE_SECONDS,
} from '../../utils/constants';
import { WorkletTelemetry } from '../../utils/telemetry';
import { BinarySemaphore } from '../../utils/semaphore';

export interface AbstractTransport {
    get outputBufferSize(): number;
    set outputBufferMaxSizeSeconds(size: number);
    push(input: Float32Array, loopback: Float32Array): void;
    pull(output: Float32Array): boolean;
    reset(): void;
}

type WorkerPort = CustomPort<WorkletMessage, WorkerMessage>;

export class MessageTransport implements AbstractTransport {
    protected readonly outputBuffer: RingBuffer;

    protected readonly inputBuffer: RingBuffer;

    protected readonly loopbackBuffer: RingBuffer;

    constructor(
        protected readonly port: WorkerPort,
        protected readonly settings: Settings,
        protected readonly telemetry: WorkletTelemetry,
        protected readonly onError: (error: Error) => void,
    ) {
        const inputBufferSize = getBufferSize(sampleRate, getInputBufferSeconds(settings));

        this.inputBuffer = new RingBuffer(inputBufferSize);
        this.loopbackBuffer = new RingBuffer(inputBufferSize);
        this.outputBuffer = new RingBuffer(getBufferSize(sampleRate, getOutputBufferSeconds(settings)));
        this.port.addEventListener('message', this.handleWorkerMessage);
    }

    get outputBufferSize(): number {
        return this.outputBuffer.size;
    }

    set outputBufferMaxSizeSeconds(value: number) {
        this.outputBuffer.maxSize = getBufferSize(sampleRate, value);
    }

    push(inputChannel: Float32Array, loopbackChannel: Float32Array) {
        this.inputBuffer.push(inputChannel);
        this.loopbackBuffer.push(loopbackChannel);
        const transferChunkSize = getBufferSize(sampleRate, TRANSPORT_SIZE_SECONDS);

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

    constructor(buffer: SharedArrayBuffer, settings: Settings) {
        let offset = 0;
        const inputSize = getBufferSize(sampleRate, getInputBufferSeconds(settings));
        const inputABSize = RingBuffer.getInternalBufferSize(inputSize);
        const outputSize = getBufferSize(sampleRate, getOutputBufferSeconds(settings));

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

    set outputBufferMaxSizeSeconds(value: number) {
        this.outputBuffer.maxSize = getBufferSize(sampleRate, value);
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
