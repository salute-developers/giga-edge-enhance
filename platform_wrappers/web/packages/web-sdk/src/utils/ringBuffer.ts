import { ASYNC_WAIT_TIMEOUT, SYNC_WAIT_TIMEOUT } from './constants';

// non blocking, one thread reads, another thread writes
export class RingBuffer {
    protected static readonly HEAD = 0;

    protected static readonly TAIL = 1;

    protected static readonly MAX_SIZE = 2;

    protected readonly data: Float32Array;

    protected readonly header: Int32Array;

    protected readonly isSharedBuffer: boolean;

    constructor(
        capacity: number,
        buffer: ArrayBufferLike = new ArrayBuffer(RingBuffer.getInternalBufferSize(capacity)),
        byteOffset = 0,
    ) {
        this.header = new Int32Array(buffer, byteOffset, 3);
        this.data = new Float32Array(buffer, byteOffset + 3 * Int32Array.BYTES_PER_ELEMENT, capacity + 1);
        this.isSharedBuffer = !(buffer instanceof ArrayBuffer);
    }

    get size(): number {
        const cap = this.data.length;

        return (this.writeIndex - this.readIndex + cap) % cap;
    }

    get free(): number {
        return Math.max(this.maxSize - this.size, 0);
    }

    get maxSize(): number {
        const defaultMaxSize = this.data.length - 1;

        if (this.isSharedBuffer) {
            return Atomics.load(this.header, RingBuffer.MAX_SIZE) || defaultMaxSize;
        }

        return this.header[RingBuffer.MAX_SIZE] || defaultMaxSize;
    }

    set maxSize(value: number) {
        const size = Math.min(Math.max(value, 0), this.data.length - 1);

        if (this.isSharedBuffer) {
            Atomics.store(this.header, RingBuffer.MAX_SIZE, size);

            return;
        }

        this.header[RingBuffer.MAX_SIZE] = size;
    }

    // eslint-disable-next-line @typescript-eslint/member-ordering
    protected get writeIndex(): number {
        if (this.isSharedBuffer) {
            return Atomics.load(this.header, RingBuffer.HEAD); // head
        }

        return this.header[RingBuffer.HEAD];
    }

    protected set writeIndex(value: number) {
        if (this.isSharedBuffer) {
            Atomics.store(this.header, RingBuffer.HEAD, value);

            return;
        }

        this.header[RingBuffer.HEAD] = value;
    }

    // eslint-disable-next-line @typescript-eslint/member-ordering
    protected get readIndex(): number {
        if (this.isSharedBuffer) {
            return Atomics.load(this.header, RingBuffer.TAIL); // tail
        }

        return this.header[RingBuffer.TAIL];
    }

    protected set readIndex(value: number) {
        if (this.isSharedBuffer) {
            Atomics.store(this.header, RingBuffer.TAIL, value);

            return;
        }

        this.header[RingBuffer.TAIL] = value;
    }

    static getInternalBufferSize(capacity: number) {
        return 3 * Int32Array.BYTES_PER_ELEMENT + (capacity + 1) * Float32Array.BYTES_PER_ELEMENT;
    }

    clear(): void {
        this.readIndex = 0;
        this.writeIndex = 0;
    }

    push(input: Float32Array): number {
        const cap = this.data.length;
        const size = Math.min(input.length, this.free);
        const { writeIndex } = this;

        if (size <= 0) return 0;

        const firstPart = Math.min(size, cap - writeIndex);
        this.data.set(input.subarray(0, firstPart), writeIndex);

        if (size > firstPart) {
            this.data.set(input.subarray(firstPart, size), 0);
        }

        const newIndex = (writeIndex + size) % cap;

        if (this.isSharedBuffer) {
            // Rewrite if not cleared only
            if (Atomics.compareExchange(this.header, RingBuffer.HEAD, writeIndex, newIndex) === writeIndex) {
                this.notify();
            }
        } else {
            this.writeIndex = newIndex;
        }

        return size;
    }

    pull(output: Float32Array): number {
        const cap = this.data.length;
        const used = this.size;
        const size = output.length;
        const { readIndex } = this;

        if (output.length > used || size <= 0) return 0;

        const firstPart = Math.min(size, cap - readIndex);
        output.set(this.data.subarray(readIndex, readIndex + firstPart), 0);

        if (size > firstPart) {
            output.set(this.data.subarray(0, size - firstPart), firstPart);
        }

        const newIndex = (readIndex + size) % cap;

        if (this.isSharedBuffer) {
            // Rewrite if not cleared only
            Atomics.compareExchange(this.header, RingBuffer.TAIL, readIndex, newIndex);
        } else {
            this.readIndex = newIndex;
        }

        return size;
    }

    async wait(): Promise<boolean> {
        if (!this.isSharedBuffer) return false;

        if (Atomics.waitAsync) {
            const result = Atomics.waitAsync(this.header, RingBuffer.HEAD, this.writeIndex, ASYNC_WAIT_TIMEOUT);

            if (!result.async) return true;
            const res = await result.value;

            return res === 'ok';
        }

        const start = performance.now();
        const value = this.writeIndex;

        while (performance.now() - start < ASYNC_WAIT_TIMEOUT) {
            const result = Atomics.wait(this.header, RingBuffer.HEAD, value, SYNC_WAIT_TIMEOUT);

            if (result !== 'timed-out') return true;

            await new Promise((r) => setTimeout(r, 0));
        }

        return false;
    }

    protected notify() {
        if (!this.isSharedBuffer) return;

        Atomics.notify(this.header, RingBuffer.HEAD, 1);
    }
}
