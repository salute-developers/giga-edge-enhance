/* eslint-disable @typescript-eslint/ban-ts-comment */
// packages/web-sdk/src/utils/__test__/semaphore.test.ts
import path from 'path';
import { Worker } from 'node:worker_threads';

import { BinarySemaphore } from '../semaphore';

jest.mock('../constants', () => ({
    ASYNC_WAIT_TIMEOUT: 200,
    MAX_SEMAPHORE_WAIT_TIME_MS: 500,
    SYNC_WAIT_TIMEOUT: 1,
}));

describe('BinarySemaphore (one instance)', () => {
    let sem: BinarySemaphore;
    beforeEach(() => {
        // the semaphore itself needs a SharedArrayBuffer; we keep it small
        const buf = new SharedArrayBuffer(BinarySemaphore.INTERNAL_BUFFER_SIZE);
        sem = new BinarySemaphore(buf);
    });

    test('lock / unlock from the same thread', async () => {
        expect(() => sem.unlock()).not.toThrow(); // unlock when unlocked
        sem.lock();
        expect(() => sem.unlock()).not.toThrow();
    });

    test('tryLock non‑blocking', async () => {
        sem.lock();
        expect(sem.tryLock()).toBe(false); // cannot acquire while locked
        sem.unlock();
        expect(sem.tryLock()).toBe(true); // succeeds after unlock
        expect(sem.tryLock()).toBe(false); // already locked again
        sem.unlock();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});

describe('BinarySemaphore (2 instances)', () => {
    // helper that creates a shared buffer and exposes two semaphores that share it
    function createSharedSems() {
        const buf = new SharedArrayBuffer(BinarySemaphore.INTERNAL_BUFFER_SIZE);
        const sem1 = new BinarySemaphore(buf);
        const sem2 = new BinarySemaphore(buf);

        return { sem1, sem2 };
    }

    test('write from one instance is visible to another', async () => {
        const { sem1, sem2 } = createSharedSems();
        sem1.lock(); // owner 1 acquires
        expect(sem2.tryLock()).toBe(false); // owner 2 cannot
        sem1.unlock(); // owner 1 releases
        expect(sem2.tryLock()).toBe(true); // now it can
    });

    test('lock timeout', async () => {
        const { sem1, sem2 } = createSharedSems();
        sem1.lock(); // owner 1 acquires

        expect(() => sem2.lock()).toThrow();

        sem1.unlock();

        expect(sem2.lock()).toBeUndefined();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});

function createWorker(script: string, workerData: Record<string, unknown>) {
    const worker = new Worker(script, { eval: true, workerData });

    function waitFor(command: string, timeout: number = 0): Promise<void> {
        return new Promise((res, rej) => {
            worker.once('message', ({ cmd }) => {
                if (command === cmd) {
                    res();
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
  const { BinarySemaphore } = require(workerData.modulePath);

  const sem = new BinarySemaphore(workerData.sharedBuffer);

  parentPort.on('message', async ({ cmd }) => {
    switch (cmd) {
      case 'lock':
        try {
            sem.lock();
        } catch {
            parentPort.postMessage({ cmd: 'timeout' });
        }
        parentPort.postMessage({ cmd: 'locked' });
        break;
      case 'unlock':
        sem.unlock();
        parentPort.postMessage({ cmd: 'unlocked' });
        break;
      default:
    }
  });
`;

describe('BinarySemaphore – master‑worker protocol', () => {
    const sharedBuffer = new SharedArrayBuffer(BinarySemaphore.INTERNAL_BUFFER_SIZE);

    const modulePath = path.resolve(__dirname, '../semaphore');

    const w1 = createWorker(workerScript, { modulePath, sharedBuffer });
    const w2 = createWorker(workerScript, { modulePath, sharedBuffer });

    test('Binary semaphore (2 workers)', async () => {
        let res = w1.waitFor('locked');
        w1.send('lock');
        expect(res).resolves.toBeUndefined();

        res = w2.waitFor('locked', 100);
        w2.send('lock');
        expect(res).rejects.toThrow();

        res = w1.waitFor('unlocked');
        let res2 = w2.waitFor('locked');
        w1.send('unlock');
        expect(res).resolves.toBeUndefined();
        expect(res2).resolves.toBeUndefined();

        res = w1.waitFor('locked', 100);
        w1.send('lock');
        expect(res).rejects.toThrow();

        res = w2.waitFor('unlocked');
        res2 = w1.waitFor('locked');
        w2.send('unlock');
        expect(res).resolves.toBeUndefined();
        expect(res2).resolves.toBeUndefined();

        res = w1.waitFor('unlocked');
        w1.send('unlock');
        expect(res).resolves.toBeUndefined();

        const sem = new BinarySemaphore(sharedBuffer);

        expect(sem.tryLock()).toBe(true);
    });

    test('Binary semaphore lock timeout', async () => {
        let res = w1.waitFor('locked');
        w1.send('lock');
        expect(res).resolves.toBeUndefined();

        res = w2.waitFor('timeout', 1000);
        w2.send('lock');
        expect(res).resolves.toBeUndefined();

        w2.send('lock');
        res = w1.waitFor('unlocked');
        const res2 = w2.waitFor('locked');
        w1.send('unlock');
        expect(res).resolves.toBeUndefined();
        expect(res2).resolves.toBeUndefined();
    });

    afterAll(() => {
        w1.worker.terminate();
        w2.worker.terminate();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
