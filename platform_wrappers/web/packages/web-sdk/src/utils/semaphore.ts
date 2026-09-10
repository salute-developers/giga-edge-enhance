import { MAX_SEMAPHORE_WAIT_TIME_MS, SYNC_WAIT_TIMEOUT } from './constants';

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
    lock(): void {
        // Just counting ms (Atomics.wait timeout) as we can't measure time correctly in AudioWorkletProcessor
        let lockOverallWaitTimeMs = 0;

        /**
         * Try locking the semaphore. If it is already locked,
         * wait for it to become available and try again.
         */
        while (!this.tryLock()) {
            if (lockOverallWaitTimeMs >= MAX_SEMAPHORE_WAIT_TIME_MS) {
                throw new Error(`Semaphore lock timed out after ${lockOverallWaitTimeMs}ms`);
            }

            const waitTimeMs = Math.min(SYNC_WAIT_TIMEOUT, MAX_SEMAPHORE_WAIT_TIME_MS - lockOverallWaitTimeMs);
            const result = Atomics.wait(
                this.header,
                BinarySemaphore.INDEX,
                BinarySemaphore.LOCKED,
                waitTimeMs,
            );

            // AudioWorklet cannot measure wall-clock time reliably. Count only
            // completed timeouts, which gives the lock a bounded wait budget.
            if (result === 'timed-out') {
                lockOverallWaitTimeMs += waitTimeMs;
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
