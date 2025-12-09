import { ASYNC_WAIT_TIMEOUT, MAX_SEMAPHORE_WAIT_TIME_MS, SYNC_WAIT_TIMEOUT } from './constants';
import { assert } from './error';

function getTime() {
    const isWorklet = typeof currentFrame !== 'undefined';

    if (isWorklet) {
        return 1000 * (currentFrame / sampleRate);
    }

    return performance.now();
}
export class BinarySemaphore {
    static readonly INTERNAL_BUFFER_SIZE = Int32Array.BYTES_PER_ELEMENT;

    protected static readonly LOCKED = 1;

    protected static readonly UNLOCKED = 0;

    protected static readonly INDEX = 0;

    protected readonly header: Int32Array;

    constructor(buffer: SharedArrayBuffer, byteOffset: number = 0) {
        this.header = new Int32Array(buffer, byteOffset, 1);
    }

    /** Non‑blocking acquire – returns true if the lock was obtained. */
    tryLock() {
        /**
         * Atomics.compareExchange returns the previous value.
         * If it was 0, set it to 1 so the lock is obtained.
         * If it was 1, that means lock is already locked.
         */
        return !Atomics.compareExchange(
            this.header,
            BinarySemaphore.INDEX,
            BinarySemaphore.UNLOCKED,
            BinarySemaphore.LOCKED,
        );
    }

    /**
     * Acquire the semaphore.
     */
    async lock(): Promise<void> {
        const lockStart = getTime();

        /**
         * Try locking the semaphore. If it is already locked,
         * wait for it to become available and try again.
         */
        while (!this.tryLock()) {
            assert(getTime() - lockStart > MAX_SEMAPHORE_WAIT_TIME_MS, 'semaphore must not lock for too long');

            if (Atomics.waitAsync) {
                const result = Atomics.waitAsync(
                    this.header,
                    BinarySemaphore.INDEX,
                    BinarySemaphore.LOCKED,
                    ASYNC_WAIT_TIMEOUT,
                );

                await result.value;
            }

            const start = getTime();

            while (getTime() - start < ASYNC_WAIT_TIMEOUT) {
                const result = Atomics.wait(
                    this.header,
                    BinarySemaphore.INDEX,
                    BinarySemaphore.LOCKED,
                    SYNC_WAIT_TIMEOUT,
                );

                if (result !== 'timed-out') continue;

                await new Promise((r) => setTimeout(r, SYNC_WAIT_TIMEOUT));
            }
        }
    }

    /**
     * Release the semaphore and wake one waiting thread (if any).
     */
    unlock() {
        Atomics.store(this.header, BinarySemaphore.INDEX, BinarySemaphore.UNLOCKED);
        Atomics.notify(this.header, BinarySemaphore.INDEX, 1);
    }
}
