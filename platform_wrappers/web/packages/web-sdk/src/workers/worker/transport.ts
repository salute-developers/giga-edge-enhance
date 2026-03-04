import { RingBuffer } from '../../utils/ringBuffer';
import { CustomPort, WORKER_MESSAGES, WorkerMessage, WORKLET_MESSAGES, WorkletMessage } from '../../utils/ipc';
import {
    getInputBufferSize,
    getOutputBufferSize,
    MAX_MESSAGE_WAIT_TIME_MS,
    MAX_SPIN_BLOCK_TIME_MS,
    TRANSPORT_SIZE_SECONDS,
} from '../../utils/constants';
import { assert } from '../../utils/error';
import { BinarySemaphore } from '../../utils/semaphore';

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

    protected timeoutId = 0 as any as NodeJS.Timeout;

    protected firstMessageInTickTime: number | null = null;

    constructor(
        protected readonly port: WorkletPort,
        protected readonly sampleRate: number,
        protected readonly frameSize: number,
        protected readonly onFrameReceived: (payload: Payload) => void,
    ) {
        this.inputBuffer = new RingBuffer(2 * getInputBufferSize(sampleRate));
        this.loopbackBuffer = new RingBuffer(2 * getInputBufferSize(sampleRate));
        this.outputBuffer = new RingBuffer(2 * Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate));
        this.port.addEventListener('message', this.handleWorkletMessage);
        this.port.start();
    }

    push(output: Float32Array): void {
        this.outputBuffer.push(output);
        const transferChunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * this.sampleRate);

        while (this.outputBuffer.size >= transferChunkSize) {
            const chunk = new Float32Array(transferChunkSize);
            this.outputBuffer.pull(chunk);

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
                if (this.firstMessageInTickTime === null) {
                    this.firstMessageInTickTime = performance.now();
                    setTimeout(() => (this.firstMessageInTickTime = null), 0);
                }

                const { input, loopback } = event.data;

                if (this.inputBuffer.free < input.length) {
                    this.inputBuffer.clear();
                    this.loopbackBuffer.clear();
                }

                this.inputBuffer.push(input);
                this.loopbackBuffer.push(loopback);

                if (performance.now() - this.firstMessageInTickTime > MAX_MESSAGE_WAIT_TIME_MS) {
                    this.process();
                    clearTimeout(this.timeoutId);
                }

                if (!this.timeoutId) {
                    this.timeoutId = setTimeout(() => this.process(), 0);
                }

                return;
            default:
        }

        assert(false, 'Unknown message type from Worklet');
    };

    protected async process() {
        const frameSize = Math.ceil(this.frameSize * this.sampleRate);
        let partitionTimestamp = performance.now();

        while (this.inputBuffer.size >= frameSize) {
            const inputBuffer = new Float32Array(frameSize);
            const loopbackBuffer = new Float32Array(frameSize);
            this.inputBuffer.pull(inputBuffer);
            this.loopbackBuffer.pull(loopbackBuffer);

            this.onFrameReceived({
                input: inputBuffer,
                loopback: loopbackBuffer,
            });

            if (performance.now() - partitionTimestamp > MAX_SPIN_BLOCK_TIME_MS) {
                await new Promise((r) => setTimeout(r, 0));
                partitionTimestamp = performance.now();
            }
        }

        this.timeoutId = 0 as any as NodeJS.Timeout;
    }
}

// size 7 * sizeof(uint32) + input buffer size * sizeof(float32) + output buffer size * sizeof(float32)
export class SharedMemoryTransport implements AbstractTransport {
    protected readonly inputBuffer: RingBuffer;

    protected readonly loopbackBuffer: RingBuffer;

    protected readonly outputBuffer: RingBuffer;

    protected readonly semaphore: BinarySemaphore;

    protected stopped = false;

    constructor(
        buffer: SharedArrayBuffer,
        protected readonly sampleRate: number,
        protected readonly frameSize: number,
        protected readonly onFrameReceived: (payload: Payload) => void,
        protected readonly onError: (error: Error) => void,
    ) {
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

        this.spin().catch(this.onError);
    }

    push(output: Float32Array): void {
        this.outputBuffer.push(output);
    }

    reset() {
        // Let the worklet reset shared buffers
    }

    close() {
        this.stopped = true;
    }

    protected async spin() {
        let partitionTimestamp = performance.now();

        while (!this.stopped) {
            const frameSize = Math.ceil(this.frameSize * this.sampleRate);
            const inputSize = this.inputBuffer.size;
            const loopbackSize = this.loopbackBuffer.size;
            const hasSufficientData = inputSize >= frameSize && loopbackSize >= frameSize;

            if (!hasSufficientData) {
                await this.inputBuffer.wait();
                partitionTimestamp = performance.now();
                continue;
            }

            const input = new Float32Array(frameSize);
            const loopback = new Float32Array(frameSize);

            this.semaphore.lock();
            const pulledInput = this.inputBuffer.pull(input);
            const pulledLoopback = this.loopbackBuffer.pull(loopback);
            this.semaphore.unlock();

            // Check for clear on worklet side case
            if (pulledInput === pulledLoopback && pulledInput === frameSize) {
                this.onFrameReceived({
                    input,
                    loopback,
                });
            }

            if (performance.now() - partitionTimestamp > MAX_SPIN_BLOCK_TIME_MS) {
                await new Promise((r) => setTimeout(r, 0));
                partitionTimestamp = performance.now();
            }
        }
    }
}
