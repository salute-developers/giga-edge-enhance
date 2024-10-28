class RingBuffer {
    readonly #capacity: number;

    readonly #buffer: Float32Array;

    #size = 0;

    #writeIndex = 0;

    #readIndex = 0;

    constructor(capacity: number) {
        this.#capacity = capacity;
        this.#buffer = new Float32Array(capacity);
    }

    get size(): number {
        return this.#size;
    }

    clear(): void {
        this.#size = 0;
        this.#readIndex = 0;
        this.#writeIndex = 0;
    }

    push(input: Float32Array, size: number = input.length): boolean {
        this.#buffer.set(input, this.#writeIndex);
        this.#writeIndex = (this.#writeIndex + size) % this.#capacity;

        let overflow = false;

        if (this.#size + size > this.#capacity) {
            overflow = true;
            this.#readIndex = (this.#readIndex + size) % this.#capacity;
        } else {
            this.#size += size;
        }

        return overflow;
    }

    pull(output: Float32Array, startIndex: number, size: number): boolean {
        if (this.#size >= size) {
            this.#size -= size;
            output.set(this.#buffer.subarray(this.#readIndex, this.#readIndex + size), startIndex);
            this.#readIndex = (this.#readIndex + size) % this.#capacity;

            return true;
        }

        return false;
    }
}

export default RingBuffer;
