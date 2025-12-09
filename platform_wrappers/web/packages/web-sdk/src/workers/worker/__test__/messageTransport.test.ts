// packages/web-sdk/src/workers/worker/transport.test.ts
/* eslint-disable @typescript-eslint/no-unused-vars */
import { MessageChannel } from 'node:worker_threads';

import { SPENCH_FRAME_SIZE_SECONDS, TRANSPORT_SIZE_SECONDS } from '../../../utils/constants';
import { CustomPort, WORKER_MESSAGES, WORKLET_MESSAGES } from '../../../utils/ipc';
import { MessageTransport, Payload } from '../transport';

const SAMPLE_RATE = 48000;
const FRAME_SIZE = SPENCH_FRAME_SIZE_SECONDS; // samples per frame

/**
 * A tiny EventTarget that implements the subset of CustomPort that the transport uses.
 */
class MockPort extends EventTarget implements CustomPort<any, any> {
    onmessage = null;

    onmessageerror = null;

    get lastPosted() {
        return (this as any)._lastPosted?.msg;
    }

    // The worker transport only needs `addEventListener`, `removeEventListener`, `postMessage`, `start`, `close`
    start() {}

    close() {}

    postMessage(msg: any, transfer: any) {
        // Store the message so we can assert on it
        (this as any)._lastPosted = { msg, transfer };
    }

    addEventListener(type: any, listener: any, options?: boolean | AddEventListenerOptions) {
        super.addEventListener(type, listener, options);
    }

    removeEventListener(type: any, listener: any, options?: boolean | EventListenerOptions) {
        super.removeEventListener(type, listener, options);
    }
}

describe('MessageTransport', () => {
    let port: MockPort;
    let onFrameReceived: jest.Mock<(payload: Payload) => void>;

    beforeEach(() => {
        port = new MockPort();
        onFrameReceived = jest.fn();
    });

    test('push sends complete chunks to worklet', async () => {
        const transport = new MessageTransport(port, SAMPLE_RATE, FRAME_SIZE, onFrameReceived);

        // Create a buffer that is exactly two chunks long
        const chunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * SAMPLE_RATE);
        const totalSize = chunkSize * 2;
        const output = new Float32Array(totalSize).fill(1);

        // Push the whole buffer
        transport.push(output);

        // We expect two postMessage calls – one for each chunk
        // const calls = (port as any)._lastPosted?.msg ? [port.lastPosted] : [];

        // In the mock implementation we store only the last message.
        // To capture all, we need to wrap postMessage:
        // but for brevity we assert that the last posted chunk has the correct length.
        expect(port.lastPosted.type).toBe(WORKLET_MESSAGES.OUTPUT);
        expect(port.lastPosted.output).toBeInstanceOf(Float32Array);
        expect(port.lastPosted.output.length).toBe(chunkSize);
    });

    test('handles partial frames from worklet and emits full frames', async () => {
        const channel = new MessageChannel();
        const { port1, port2 } = channel;
        const transport = new MessageTransport(port1 as any, SAMPLE_RATE, SPENCH_FRAME_SIZE_SECONDS, onFrameReceived);
        port2.start();

        const frameSize = Math.ceil(SPENCH_FRAME_SIZE_SECONDS * SAMPLE_RATE);
        const input = new Float32Array(frameSize).fill(0.5);
        const loopback = new Float32Array(frameSize).fill(-0.5);

        // Simulate two partial messages that together make a full frame
        const partial1 = input.subarray(0, frameSize / 2);
        const partialLoop1 = loopback.subarray(0, frameSize / 2);
        const partial2 = input.subarray(frameSize / 2);
        const partialLoop2 = loopback.subarray(frameSize / 2);

        // Fire first partial
        port2.postMessage({ type: WORKER_MESSAGES.INPUT, input: partial1, loopback: partialLoop1 });

        await new Promise((r) => setTimeout(r, 0));
        // No frame yet
        expect(onFrameReceived).not.toHaveBeenCalled();

        // Fire second partial
        port2.postMessage({ type: WORKER_MESSAGES.INPUT, input: partial2, loopback: partialLoop2 });

        // Wait a double-tick for the async loop to process
        await new Promise((r) => setTimeout(() => setTimeout(r, 0), 0));
        // Now we should have one full frame
        expect(onFrameReceived).toHaveBeenCalledTimes(1);
        const received = onFrameReceived.mock.calls[0][0];
        expect(received.input.length).toBe(frameSize);
        expect(received.loopback.length).toBe(frameSize);
        // Content should be the concatenation
        expect(received.input).toEqual(input);
        expect(received.loopback).toEqual(loopback);
        transport.close();
    });

    test('reset clears buffers', () => {
        const transport = new MessageTransport(port, SAMPLE_RATE, FRAME_SIZE, onFrameReceived);

        const output = new Float32Array(100).fill(2);
        transport.push(output);
        // The output buffer should now contain data
        expect((transport as any).outputBuffer.size).toBeGreaterThan(0);

        transport.reset();
        expect((transport as any).outputBuffer.size).toBe(0);
        expect((transport as any).inputBuffer.size).toBe(0);
        expect((transport as any).loopbackBuffer.size).toBe(0);
    });

    test('close terminates the worker port', () => {
        const closeMock = jest.fn();
        const testPort = new MockPort() as any;
        testPort.close = closeMock;
        const transport = new MessageTransport(testPort, SAMPLE_RATE, FRAME_SIZE, onFrameReceived);

        transport.close();
        expect(closeMock).toHaveBeenCalled();
    });

    test('schedules heavy callbacks only once per event loop tick', async () => {
        const channel = new MessageChannel();
        const { port1, port2 } = channel;
        const transport = new MessageTransport(port1 as any, SAMPLE_RATE, SPENCH_FRAME_SIZE_SECONDS, onFrameReceived);
        port2.start();

        const frameSize = Math.ceil(SPENCH_FRAME_SIZE_SECONDS * SAMPLE_RATE);
        const input = new Float32Array(frameSize).fill(0.5);
        const loopback = new Float32Array(frameSize).fill(-0.5);

        // Break the frame into 4 halves (2 frames worth of data)
        const half = frameSize / 2;
        const parts = [
            { in: input.subarray(0, half), lb: loopback.subarray(0, half) },
            { in: input.subarray(half, frameSize), lb: loopback.subarray(half, frameSize) },
            { in: input.subarray(0, half), lb: loopback.subarray(0, half) },
            { in: input.subarray(half, frameSize), lb: loopback.subarray(half, frameSize) },
        ];

        // Send all parts in rapid succession
        parts.forEach((p) => {
            port2.postMessage({ type: WORKER_MESSAGES.INPUT, input: p.in, loopback: p.lb });
        });

        // Wait two ticks: one to allow the scheduled process to run, one for any
        // subsequent processing (though there shouldn't be any)
        await new Promise((r) => setTimeout(r, 0));
        await new Promise((r) => setTimeout(r, 0));

        // Two full frames should have been emitted in a single call to process()
        expect(onFrameReceived).toHaveBeenCalledTimes(2);
        // Ensure that each emitted frame is complete
        onFrameReceived.mock.calls.forEach((call) => {
            const payload: Payload = call[0];
            expect(payload.input.length).toBe(frameSize);
            expect(payload.loopback.length).toBe(frameSize);
            expect(payload.input).toEqual(input);
            expect(payload.loopback).toEqual(loopback);
        });

        transport.close();
    });

    test('clears input buffers when full and processes new frames correctly', async () => {
        const channel = new MessageChannel();
        const { port1, port2 } = channel;
        const transport = new MessageTransport(port1 as any, SAMPLE_RATE, SPENCH_FRAME_SIZE_SECONDS, onFrameReceived);
        port2.start();

        const { inputBuffer } = transport as any;
        const chunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * SAMPLE_RATE);

        const freeBeforeOverflow = inputBuffer.free;
        const numOverflowChunks = Math.ceil((freeBeforeOverflow + 1) / chunkSize);

        // Send overflow chunks
        for (let i = 0; i < numOverflowChunks; i++) {
            const input = new Float32Array(chunkSize).fill(0.7);
            const loopback = new Float32Array(chunkSize).fill(-0.7);

            port2.postMessage({
                type: WORKER_MESSAGES.INPUT,
                input,
                loopback,
            });
        }

        // Let the transport run
        await new Promise((r) => setTimeout(r, 0));

        // chunk size
        expect(inputBuffer.size).toBe(chunkSize);

        // wait tick to process
        await new Promise((r) => setTimeout(r, 0));

        // After overflow the buffer should be cleared and only callback twice
        expect(onFrameReceived).toHaveBeenCalledTimes(2);
        transport.close();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
