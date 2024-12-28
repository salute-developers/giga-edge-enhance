class RingBuffer {
    readonly buffer: Float32Array;

    writeIndex = 0;

    readIndex = 0;

    #size = 0;

    constructor(capacity: number) {
        this.buffer = new Float32Array(capacity);
    }

    get size(): number {
        return this.#size;
    }

    clear(): void {
        this.#size = 0;
        this.readIndex = 0;
        this.writeIndex = 0;
    }

    push(input: Float32Array, size: number = input.length): boolean {
        let written = 0;

        while (size - written > 0) {
            const currentChunkSize = Math.min(this.buffer.length - this.writeIndex, size - written);
            this.buffer.set(input.subarray(written, written + currentChunkSize), this.writeIndex);
            written += currentChunkSize;
            this.writeIndex = (this.writeIndex + currentChunkSize) % this.buffer.length;
        }

        let overflow = false;

        if (this.#size + size > this.buffer.length) {
            overflow = true;
            this.readIndex = (this.readIndex + size) % this.buffer.length;
        } else {
            this.#size += size;
        }

        return overflow;
    }

    pull(output: Float32Array, startIndex: number, size: number): boolean {
        if (this.#size >= size) {
            let written = 0;

            while (size - written > 0) {
                const currentChunkSize = Math.min(this.buffer.length - this.readIndex, size - written);
                output.set(
                    this.buffer.subarray(this.readIndex, this.readIndex + currentChunkSize),
                    startIndex + written,
                );
                written += currentChunkSize;
                this.readIndex = (this.readIndex + currentChunkSize) % this.buffer.length;
            }

            this.#size -= size;

            return true;
        }

        return false;
    }
}

export default RingBuffer;
