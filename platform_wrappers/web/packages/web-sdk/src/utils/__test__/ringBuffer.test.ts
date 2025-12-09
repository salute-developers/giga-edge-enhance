import path from 'path';
import { Worker } from 'node:worker_threads';

import { RingBuffer } from '../ringBuffer';

describe('RingBuffer (non‑shared)', () => {
    const CAPACITY = 8; // usable items
    let buffer: RingBuffer;

    beforeEach(() => {
        buffer = new RingBuffer(CAPACITY);
    });

    test('push and pull a small array', () => {
        const input = new Float32Array([1, 2, 3]);
        expect(buffer.push(input)).toBe(3);
        expect(buffer.size).toBe(3);

        const output = new Float32Array(3);
        expect(buffer.pull(output)).toBe(3);
        expect(output).toEqual(input);
        expect(buffer.size).toBe(0);
    });

    test('cannot push more than capacity', () => {
        const fullInput = new Float32Array(CAPACITY + 2).fill(1);
        // only CAPACITY items should be accepted
        expect(buffer.push(fullInput)).toBe(CAPACITY);
        expect(buffer.size).toBe(CAPACITY);
    });

    test('wrap‑around behaviour', () => {
        const firstChunk = new Float32Array(CAPACITY - 2).fill(9);
        expect(buffer.push(firstChunk)).toBe(firstChunk.length);
        expect(buffer.size).toBe(firstChunk.length);

        // read part of it
        const out1 = new Float32Array(3);
        expect(buffer.pull(out1)).toBe(3);
        expect(buffer.size).toBe(firstChunk.length - 3);

        // push again so that it wraps
        const secondChunk = new Float32Array(5).fill(7);
        expect(buffer.push(secondChunk)).toBe(5);
        expect(buffer.size).toBe(firstChunk.length - 3 + 5);

        // pull all remaining data
        const outAll = new Float32Array(buffer.size);
        expect(buffer.pull(outAll)).toBe(outAll.length);
        expect(buffer.size).toBe(0);

        // Check the order
        const expected = new Float32Array([...firstChunk.slice(3), ...secondChunk]);
        expect(outAll).toEqual(expected);
    });

    test('clear resets the buffer', () => {
        buffer.push(new Float32Array([4, 5, 6]));
        buffer.clear();
        expect(buffer.size).toBe(0);
        expect(buffer.pull(new Float32Array(3))).toBe(0);
    });

    test('maxSize setter clamps values', () => {
        expect(buffer.maxSize).toBe(CAPACITY);

        buffer.maxSize = CAPACITY + 10;
        expect(buffer.maxSize).toBe(CAPACITY); // clamped to max

        buffer.maxSize = CAPACITY - 5;
        expect(buffer.maxSize).toBe(CAPACITY - 5); // not clamped

        buffer.maxSize = -5;
        expect(buffer.maxSize).toBe(CAPACITY); // clamped to 0 which default to max
    });

    test('size and free getters are consistent', () => {
        const in1 = new Float32Array([1, 2, 3, 4]);
        buffer.push(in1);
        expect(buffer.size).toBe(in1.length);
        expect(buffer.free).toBe(CAPACITY - in1.length);

        buffer.pull(new Float32Array(1));
        expect(buffer.size).toBe(in1.length - 1);
        expect(buffer.free).toBe(CAPACITY - (in1.length - 1));
    });

    test('push with empty array returns 0', () => {
        const empty = new Float32Array(0);
        expect(buffer.push(empty)).toBe(0);
        expect(buffer.size).toBe(0);
    });

    test('pull with empty output returns 0', () => {
        buffer.push(new Float32Array([5, 6]));
        const out = new Float32Array(0);
        expect(buffer.pull(out)).toBe(0);
        expect(buffer.size).toBe(2); // nothing was removed
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});

describe('RingBuffer (shared buffer)', () => {
    const CAPACITY = 5;

    // a helper that creates a shared buffer and exposes the two RingBuffers
    function createSharedBuffers() {
        const sb = new SharedArrayBuffer(RingBuffer.getInternalBufferSize(CAPACITY));
        const buf1 = new RingBuffer(CAPACITY, sb);
        const buf2 = new RingBuffer(CAPACITY, sb);

        return { buf1, buf2 };
    }

    test('write from one instance is visible to another', () => {
        const { buf1, buf2 } = createSharedBuffers();
        const data = new Float32Array([10, 20, 30]);

        expect(buf1.push(data)).toBe(data.length);
        expect(buf2.size).toBe(data.length);

        const out = new Float32Array(3);
        expect(buf2.pull(out)).toBe(3);
        expect(out).toEqual(data);
        expect(buf1.size).toBe(0);
    });

    test('wait() returns false when timeout expires', async () => {
        const { buf1 } = createSharedBuffers();
        // No data written – wait should timeout
        const result = await buf1.wait();
        expect(result).toBe(false);
    });

    test('clear() works on shared buffer', () => {
        const { buf1, buf2 } = createSharedBuffers();
        buf1.push(new Float32Array([10, 20]));
        expect(buf2.size).toBe(2);
        buf2.clear();
        expect(buf1.size).toBe(0);
        expect(buf2.size).toBe(0);
    });

    test('wait() returns true when notified', async () => {
        const { buf1, buf2 } = createSharedBuffers();
        const waiter = buf1.wait();
        buf2.push(new Float32Array([7, 8, 9]));
        const ok = await waiter;
        expect(ok).toBe(true);
    });

    test('wait() uses Atomics.waitAsync when available', async () => {
        const { buf1 } = createSharedBuffers();

        const original = (global as any).Atomics.waitAsync;
        try {
            (global as any).Atomics.waitAsync = (
                _view: Int32Array,
                _index: number,
                _value: number,
                _timeout: number,
            ) => {
                return {
                    async: true,
                    value: Promise.resolve('ok'),
                };
            };

            const result = await buf1.wait();
            expect(result).toBe(true);
        } finally {
            (global as any).Atomics.waitAsync = original;
        }
    });

    test('wait() returns true when a push notifies the reader', async () => {
        const { buf1, buf2 } = createSharedBuffers();
        const waiter = buf1.wait(); // start waiting
        buf2.push(new Float32Array([1, 2, 3])); // push data on the other instance
        const ok = await waiter;
        expect(ok).toBe(true);
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});

function createWorker(script: string, workerData: Record<string, unknown>) {
    const worker = new Worker(script, { eval: true, workerData });

    function waitFor(command: string, timeout: number = 0): Promise<any> {
        return new Promise((res, rej) => {
            worker.once('message', ({ cmd, value }) => {
                if (command === cmd) {
                    res(value);
                }
            });

            if (timeout > 0) {
                setTimeout(() => rej, timeout);
            }
        });
    }

    function send(cmd: string) {
        worker.postMessage({ cmd });
    }

    return { worker, send, waitFor };
}

const workerScript = `
  // Compile TS on the fly
  process.env.TS_NODE_COMPILER_OPTIONS = JSON.stringify({ module: 'CommonJS' });
  require('ts-node/register/transpile-only');

  const { parentPort, workerData } = require('worker_threads');
  const { RingBuffer } = require(workerData.modulePath);

  const buffer = new RingBuffer(workerData.capacity, workerData.sharedBuffer);
  const data = new Float32Array([1,2,3,4,5,6,7,8]);

  // doesn't work for some reason when compiled to commonjs. use sync one
  Atomics.waitAsync = undefined;

  parentPort.on('message', async ({ cmd }) => {
    switch (cmd) {
      case 'ping':
        parentPort.postMessage({ cmd: 'pong' });
        break;
      case 'push':
        const pushed = buffer.push(data);
        parentPort.postMessage({ cmd: 'pushed', value: { pushed } });
        break;
      case 'pull':
        const okPromise = buffer.wait();          // true if notified
        parentPort.postMessage({ cmd: 'pulling' });
        const ok = await okPromise;
        const used = buffer.size;                // items that should be there
        const out = new Float32Array(used);

        // Pull everything out
        const pulled = buffer.pull(out);

        parentPort.postMessage({
            cmd: 'pulled',
            value: {
                ok,
                pulled,
                out: Array.from(out),
            },
        });
        break;
      default:
    }
  });
`;

describe('RingBuffer (SharedArrayBuffer) with worker threads', () => {
    const CAPACITY = 8;

    // The raw buffer that will be shared by the two workers
    const sharedBuffer = new SharedArrayBuffer(RingBuffer.getInternalBufferSize(CAPACITY));
    const modulePath = path.resolve(__dirname, '../ringBuffer.ts');

    const w1 = createWorker(workerScript, {
        modulePath,
        capacity: CAPACITY,
        sharedBuffer,
    });
    const w2 = createWorker(workerScript, {
        modulePath,
        capacity: CAPACITY,
        sharedBuffer,
    });

    it('pushes data in one worker, reads it in another', async () => {
        // Wait for both workers to be ready
        let res1 = w1.waitFor('pong');
        let res2 = w2.waitFor('pong');
        w1.send('ping');
        w2.send('ping');
        expect(res1).resolves.toBeUndefined();
        expect(res2).resolves.toBeUndefined();

        res1 = w2.waitFor('pulling');
        w2.send('pull');
        expect(res1).resolves.toBeUndefined();
        res1 = w1.waitFor('pushed');
        res2 = w2.waitFor('pulled');
        w1.send('push');
        expect(res1).resolves.toMatchObject({ pushed: CAPACITY });
        expect(res2).resolves.toMatchObject({
            ok: true,
            pulled: CAPACITY,
            out: [1, 2, 3, 4, 5, 6, 7, 8],
        });

        res1 = w2.waitFor('pushed');
        w2.send('push');
        expect(res1).resolves.toMatchObject({ pushed: CAPACITY });

        res1 = w1.waitFor('pulled');
        w1.send('pull');
        expect(res1).resolves.toMatchObject({
            ok: false,
            pulled: CAPACITY,
            out: [1, 2, 3, 4, 5, 6, 7, 8],
        });
    });

    afterAll(() => {
        w1.worker.terminate();
        w2.worker.terminate();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
