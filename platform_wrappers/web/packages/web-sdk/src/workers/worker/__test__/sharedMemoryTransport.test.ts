/* eslint-disable no-var */
// packages/web-sdk/src/workers/worker/sharedMemoryTransport.test.ts
/* eslint-disable @typescript-eslint/no-unused-vars */
import { getInputBufferSize, getOutputBufferSize, SPENCH_FRAME_SIZE_SECONDS } from '../../../utils/constants';
import { RingBuffer } from '../../../utils/ringBuffer';
import { SharedMemoryTransport, Payload } from '../transport';

const SAMPLE_RATE = 48000;
const FRAME_SIZE = SPENCH_FRAME_SIZE_SECONDS; // samples per frame

// ---- Mock the BinarySemaphore implementation ----
var semaphoreLockMock: jest.Mock;
var semaphoreUnlockMock: jest.Mock;

jest.mock('../../../utils/semaphore', () => {
    return {
        BinarySemaphore: jest.fn().mockImplementation(() => {
            return {
                lock: (semaphoreLockMock = jest.fn()),
                unlock: (semaphoreUnlockMock = jest.fn()),
            };
        }),
    };
});

/**
 * Helper that creates a SharedArrayBuffer with the exact layout expected by
 * SharedMemoryTransport.  This mirrors the logic in the constructor so that
 * tests stay in sync with the production code.
 */
function createSharedBuffer(sampleRate: number): SharedArrayBuffer {
    const inputSize = getInputBufferSize(sampleRate);
    const inputABSize = RingBuffer.getInternalBufferSize(inputSize);
    const outputSize = getOutputBufferSize(sampleRate);
    const outputABSize = RingBuffer.getInternalBufferSize(outputSize);
    const semaphoreSize = Int32Array.BYTES_PER_ELEMENT;

    const totalSize = inputABSize + inputABSize + outputABSize + semaphoreSize;

    return new SharedArrayBuffer(totalSize);
}

describe('SharedMemoryTransport', () => {
    let buffer: SharedArrayBuffer;
    let onFrameReceived: jest.Mock<(payload: Payload) => void>;
    let transport: SharedMemoryTransport;

    beforeEach(() => {
        buffer = createSharedBuffer(SAMPLE_RATE);
        onFrameReceived = jest.fn();
        transport = new SharedMemoryTransport(buffer, SAMPLE_RATE, FRAME_SIZE, onFrameReceived);
    });

    test('push outputs are written to the ring buffer', () => {
        const out = new Float32Array(100).fill(7);
        transport.push(out);
        // The output buffer should now contain data
        expect((transport as any).outputBuffer.size).toBe(out.length);
    });

    test('reset clears all buffers', async () => {
        const out = new Float32Array(50).fill(3);
        transport.push(out);
        // Write some input data directly (bypassing worker)
        (transport as any).inputBuffer.push(out);
        (transport as any).loopbackBuffer.push(out);

        await transport.reset();

        expect((transport as any).outputBuffer.size).toBe(0);
        expect((transport as any).inputBuffer.size).toBe(0);
        expect((transport as any).loopbackBuffer.size).toBe(0);

        expect(semaphoreLockMock).toHaveBeenCalledTimes(1);
        expect(semaphoreUnlockMock).toHaveBeenCalledTimes(1);
    });

    test('close stops the spin loop', async () => {
        // const closeSpy = jest.spyOn(transport as any, 'spin').mockImplementation(async () => {
        //     /* do nothing */
        // });
        transport.close();
        expect((transport as any).stopped).toBeTruthy();
    });

    test('spins and emits a frame when sufficient data is present', async () => {
        const frameSize = Math.ceil(FRAME_SIZE * SAMPLE_RATE);

        const inputData = new Float32Array(frameSize).fill(1.1);
        const loopbackData = new Float32Array(frameSize).fill(2.2);

        // Write data into the protected buffers directly
        (transport as any).inputBuffer.push(inputData);
        (transport as any).loopbackBuffer.push(loopbackData);

        (transport as any).inputBuffer.notify();
        // Wait a double-tick for the async spin loop to process
        await new Promise((r) => setTimeout(() => setTimeout(r, 0), 0));

        expect(onFrameReceived).toHaveBeenCalledTimes(1);
        const received = onFrameReceived.mock.calls[0][0];
        expect(received.input).toEqual(inputData);
        expect(received.loopback).toEqual(loopbackData);

        expect(semaphoreLockMock).toHaveBeenCalledTimes(1);
        expect(semaphoreUnlockMock).toHaveBeenCalledTimes(1);
    });

    test('spin does not emit when data is insufficient', async () => {
        const frameSize = Math.ceil(FRAME_SIZE * SAMPLE_RATE);
        const half = Math.floor(frameSize / 2);

        const inputData = new Float32Array(half).fill(1.1);
        const loopbackData = new Float32Array(half).fill(2.2);

        (transport as any).inputBuffer.push(inputData);
        (transport as any).loopbackBuffer.push(loopbackData);

        // Wait a double-tick for the async spin loop to process
        await new Promise((r) => setTimeout(() => setTimeout(r, 0), 0));

        expect(onFrameReceived).not.toHaveBeenCalled();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
