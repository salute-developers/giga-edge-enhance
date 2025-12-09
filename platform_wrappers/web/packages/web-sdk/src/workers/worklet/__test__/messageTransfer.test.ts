/* eslint-disable @typescript-eslint/no-explicit-any */
import { MessageTransport } from '../transport';
import { WORKER_MESSAGES, WORKLET_MESSAGES } from '../../../utils/ipc';
import { TRANSPORT_SIZE_SECONDS } from '../../../utils/constants';
import { getWorkletTelemetry } from '../../../utils/telemetry';

// ---------- Helpers ----------
class DummyPort {
    private listeners = new Map<string, any>();

    addEventListener(type: string, cb: any) {
        this.listeners.set(type, cb);
    }

    // Simulate `postMessage`. We call the registered message listener immediately.
    postMessage(msg: any) {
        const handler = this.listeners.get('message');

        if (handler) {
            // eslint-disable-next-line @typescript-eslint/no-unsafe-argument
            handler({ data: msg });
        }
    }
}

// ---------- Test suite ----------
describe('MessageTransport', () => {
    const sampleRate = 48000;
    // Keep the global sampleRate for the code under test
    beforeAll(() => {
        (globalThis as any).sampleRate = sampleRate;
    });

    const makeTransport = () => {
        const port = new DummyPort();
        const telemetry = getWorkletTelemetry(true, port as any);
        const telemetrySpy = jest.spyOn(telemetry, 'frameReceived');

        const onError = jest.fn();
        const transport = new MessageTransport(port as any, telemetry, onError);

        return { transport, port, telemetry, telemetrySpy, onError };
    };

    it('pushes data in chunks of TRANSPORT_SIZE_SECONDS', () => {
        const { transport, port } = makeTransport();

        // Prepare input data larger than the transfer chunk
        const chunkSize = Math.ceil(0.5 * TRANSPORT_SIZE_SECONDS * sampleRate);
        const inputChunk = new Float32Array(chunkSize * 2); // two frames
        const loopbackChunk = new Float32Array(chunkSize * 2);
        inputChunk.fill(0.1);
        loopbackChunk.fill(0.2);

        // Keep a reference to the postMessage spy
        const postSpy = jest.spyOn(port, 'postMessage');

        // Push data in 2× chunkSize increments
        // After the first push nothing should be sent yet
        transport.push(inputChunk.slice(0, chunkSize), loopbackChunk.slice(0, chunkSize));
        expect(postSpy).not.toHaveBeenCalled();

        // After the second push the buffer will contain one full chunk and should be sent
        transport.push(inputChunk.slice(chunkSize), loopbackChunk.slice(chunkSize));
        expect(postSpy).toHaveBeenCalledTimes(1);
        const sentMsg = postSpy.mock.calls[0][0];
        expect(sentMsg.type).toBe(WORKER_MESSAGES.INPUT);
        expect(sentMsg.input.length).toBe(chunkSize * 2);
        expect(sentMsg.loopback.length).toBe(chunkSize * 2);
    });

    it('pulls data when enough output is available', () => {
        const { transport, port } = makeTransport();

        const outputSize = 256;
        const output = new Float32Array(outputSize);

        // Nothing in buffer yet, pull should return false
        expect(transport.pull(output)).toBe(false);

        // Simulate a worker sending data by calling the message listener directly
        const generated = new Float32Array(outputSize).fill(0.5);
        port.postMessage({ type: WORKLET_MESSAGES.OUTPUT, output: generated });

        // Now pull should succeed
        expect(transport.pull(output)).toBe(true);
        expect(output[0]).toBeCloseTo(0.5);
        expect(output[output.length - 1]).toBeCloseTo(0.5);
    });

    it('reset clears buffers', () => {
        const { transport, port } = makeTransport();

        const chunkSize = Math.ceil(TRANSPORT_SIZE_SECONDS * sampleRate);
        const input = new Float32Array(chunkSize);
        const loopback = new Float32Array(chunkSize);

        // Push enough data to trigger a postMessage
        transport.push(input, loopback);
        transport.push(input, loopback);

        // Ensure there is data in the output buffer
        const fakeOutput = new Float32Array(chunkSize);
        port.postMessage({ type: WORKLET_MESSAGES.OUTPUT, output: fakeOutput });

        // Reset should clear everything
        transport.reset();

        // After reset pull should still fail
        expect(transport.pull(fakeOutput)).toBe(false);
    });

    it('handles unexpected worker messages gracefully', () => {
        const { port, onError } = makeTransport();

        // Send a message with an unknown type
        port.postMessage({ type: 'UNKNOWN' });

        // onError should not have been called
        expect(onError).not.toHaveBeenCalled();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
