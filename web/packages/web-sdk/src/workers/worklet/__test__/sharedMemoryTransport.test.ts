/* eslint-disable no-var */
/* eslint-disable @typescript-eslint/no-explicit-any */
import { getSettingsWithDefaults } from '../../../utils/common';
import { getBufferSize, getOutputBufferSeconds, GDN_FRAME_SIZE_SECONDS } from '../../../utils/constants';
import { RingBuffer } from '../../../utils/ringBuffer';
import { SharedMemoryTransport } from '../transport';

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

describe('SharedMemoryTransport', () => {
    const sampleRate = 48000;
    const settings = getSettingsWithDefaults();
    beforeAll(() => {
        (globalThis as any).sampleRate = sampleRate;
    });

    const createTransport = () => {
        const inputSize = getBufferSize(sampleRate, GDN_FRAME_SIZE_SECONDS * 2);
        const inputABSize = RingBuffer.getInternalBufferSize(inputSize);

        const outputSize = getBufferSize(sampleRate, getOutputBufferSeconds(settings));
        const outputABSize = RingBuffer.getInternalBufferSize(outputSize);

        const bufferSize =
            inputABSize * 2 + // input + loopback
            outputABSize +
            Int32Array.BYTES_PER_ELEMENT; // semaphore flag

        const sab = new SharedArrayBuffer(bufferSize);

        const transport = new SharedMemoryTransport(sab, settings);

        return { transport, sab };
    };

    it('push writes to buffers even when full', async () => {
        const { transport } = createTransport();

        const inputSize = getBufferSize(sampleRate, GDN_FRAME_SIZE_SECONDS * 2);
        const inputData = new Float32Array(inputSize).fill(0.1);
        const loopbackData = new Float32Array(inputSize).fill(0.2);

        // First push should succeed
        expect(transport.push(inputData, loopbackData)).toBeUndefined();

        // Now push a second buffer that would overflow the input buffer
        const largerInput = new Float32Array(inputSize + 1).fill(0.3);
        const largerLoopback = new Float32Array(inputSize + 1).fill(0.4);

        expect(transport.push(largerInput, largerLoopback)).toBeUndefined();

        // Verify semaphore was used
        expect((transport as any).semaphore.lock).toHaveBeenCalledTimes(0);
        expect((transport as any).semaphore.unlock).toHaveBeenCalledTimes(0);
    });

    it('pull reads from the output buffer', () => {
        const { transport } = createTransport();

        const outputSize = getBufferSize(sampleRate, getOutputBufferSeconds(settings));
        const output = new Float32Array(outputSize);

        // Initially pull should fail
        expect(transport.pull(output)).toBe(false);

        // Manually push data into the output buffer via the underlying RingBuffer
        // (this is allowed because the buffer is public in the test context)
        const outputBuffer = (transport as any).outputBuffer as RingBuffer;
        const data = new Float32Array(outputSize).fill(0.7);
        outputBuffer.push(data);

        // Now pull should succeed
        expect(transport.pull(output)).toBe(true);
        expect(output[0]).toBeCloseTo(0.7);
        expect(output[output.length - 1]).toBeCloseTo(0.7);
    });

    it('reset clears all buffers', () => {
        const { transport } = createTransport();

        const inputSize = getBufferSize(sampleRate, GDN_FRAME_SIZE_SECONDS * 2);
        const inputData = new Float32Array(inputSize).fill(0.5);
        const loopbackData = new Float32Array(inputSize).fill(0.6);

        transport.push(inputData, loopbackData);

        const outputSize = getBufferSize(sampleRate, getOutputBufferSeconds(settings));
        const output = new Float32Array(outputSize);
        const outputBuffer = (transport as any).outputBuffer as RingBuffer;
        outputBuffer.push(new Float32Array(outputSize).fill(0.9));

        transport.reset();

        // After reset no data should be available
        expect(transport.pull(output)).toBe(false);
        expect(outputBuffer.size).toBe(0);
        expect((transport as any).inputBuffer.size).toBe(0);
        expect((transport as any).loopbackBuffer.size).toBe(0);
        expect((transport as any).semaphore.lock).toHaveBeenCalledTimes(1);
        expect((transport as any).semaphore.unlock).toHaveBeenCalledTimes(1);
    });
});
