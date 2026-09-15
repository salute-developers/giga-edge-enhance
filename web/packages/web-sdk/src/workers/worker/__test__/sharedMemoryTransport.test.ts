/* eslint-disable no-var */
// packages/web-sdk/src/workers/worker/sharedMemoryTransport.test.ts
/* eslint-disable @typescript-eslint/no-unused-vars */
import { getSettingsWithDefaults } from '../../../utils/common';
import {
    getBufferSize,
    getInputBufferSeconds,
    getOutputBufferSeconds,
    GDN_FRAME_SIZE_SECONDS,
} from '../../../utils/constants';
import { RingBuffer } from '../../../utils/ringBuffer';
import { SharedMemoryTransport, Payload } from '../transport';

const SAMPLE_RATE = 48000;
const FRAME_SIZE = GDN_FRAME_SIZE_SECONDS;
const SETTINGS = getSettingsWithDefaults();

jest.mock('../../../utils/semaphore', () => {
    return {
        BinarySemaphore: jest.fn().mockImplementation(() => {
            return {
                lock: jest.fn(),
                unlock: jest.fn(),
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
    const inputSize = getBufferSize(sampleRate, getInputBufferSeconds(SETTINGS));
    const inputABSize = RingBuffer.getInternalBufferSize(inputSize);
    const outputSize = getBufferSize(sampleRate, getOutputBufferSeconds(SETTINGS));
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
        transport = new SharedMemoryTransport(buffer, SAMPLE_RATE, SETTINGS, onFrameReceived, () => {});
    });

    test('push outputs are written to the ring buffer', () => {
        const out = new Float32Array(100).fill(7);
        transport.push(out);
        // The output buffer should now contain data
        expect((transport as any).outputBuffer.size).toBe(out.length);
    });

    test('close stops the spin loop', async () => {
        // const closeSpy = jest.spyOn(transport as any, 'spin').mockImplementation(async () => {
        //     /* do nothing */
        // });
        transport.close();
        expect((transport as any).stopped).toBeTruthy();
    });

    test('spins and emits a frame when sufficient data is present', async () => {
        const frameSize = getBufferSize(SAMPLE_RATE, FRAME_SIZE);

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

        expect((transport as any).semaphore.lock).toHaveBeenCalledTimes(1);
        expect((transport as any).semaphore.unlock).toHaveBeenCalledTimes(1);
    });

    test('spin does not emit when data is insufficient', async () => {
        const frameSize = getBufferSize(SAMPLE_RATE, FRAME_SIZE);
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
