import path from 'node:path';
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

const workerScript = `
    process.env.TS_NODE_COMPILER_OPTIONS = JSON.stringify({ module: 'CommonJS' });
    require('ts-node/register/transpile-only');

    const { parentPort, workerData } = require('worker_threads');
    const { BinarySemaphore } = require(workerData.modulePath);
    const semaphore = new BinarySemaphore(workerData.buffer);

    parentPort.on('message', ({ command }) => {
        if (command === 'acquire') {
            parentPort.postMessage({ type: 'attempting' });
            try {
                semaphore.lock();
                parentPort.postMessage({ type: 'acquired' });
            } catch (error) {
                parentPort.postMessage({ type: 'timeout', message: error.message });
            }
        } else if (command === 'release') {
            semaphore.unlock();
            parentPort.postMessage({ type: 'released' });
        }
    });
`;

function waitFor(worker: Worker, type: string, timeoutMs = 2_000): Promise<void> {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
            worker.off('message', handler);
            reject(new Error(`Timed out waiting for worker message '${type}'`));
        }, timeoutMs);
        const handler = (message: { type: string }) => {
            if (message.type !== type) return;
            clearTimeout(timer);
            worker.off('message', handler);
            resolve();
        };
        worker.on('message', handler);
    });
}

describe('BinarySemaphore (worker synchronization)', () => {
    const buffer = new SharedArrayBuffer(BinarySemaphore.INTERNAL_BUFFER_SIZE);
    const modulePath = path.resolve(__dirname, '../semaphore');
    let first: Worker;
    let second: Worker;

    beforeEach(() => {
        first = new Worker(workerScript, { eval: true, workerData: { buffer, modulePath } });
        second = new Worker(workerScript, { eval: true, workerData: { buffer, modulePath } });
    });

    afterEach(async () => {
        await Promise.all([first.terminate(), second.terminate()]);
    });

    test('unlock wakes a worker waiting for the semaphore', async () => {
        const firstAcquired = waitFor(first, 'acquired');
        first.postMessage({ command: 'acquire' });
        await firstAcquired;

        const secondAttempting = waitFor(second, 'attempting');
        second.postMessage({ command: 'acquire' });
        await secondAttempting;

        const secondAcquired = waitFor(second, 'acquired');
        const firstReleased = waitFor(first, 'released');
        first.postMessage({ command: 'release' });
        await Promise.all([firstReleased, secondAcquired]);

        const secondReleased = waitFor(second, 'released');
        second.postMessage({ command: 'release' });
        await secondReleased;
    });

    test('a contended lock reports a bounded timeout', async () => {
        const firstAcquired = waitFor(first, 'acquired');
        first.postMessage({ command: 'acquire' });
        await firstAcquired;

        const secondAttempting = waitFor(second, 'attempting');
        const secondTimedOut = waitFor(second, 'timeout');
        second.postMessage({ command: 'acquire' });
        await Promise.all([secondAttempting, secondTimedOut]);

        const firstReleased = waitFor(first, 'released');
        first.postMessage({ command: 'release' });
        await firstReleased;
    });
});
