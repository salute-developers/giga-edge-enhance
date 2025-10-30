class RingBuffer {
    protected readonly buffer: Float32Array;

    protected writeIndex = 0;

    protected readIndex = 0;

    protected currentSize = 0;

    constructor(capacity: number) {
        this.buffer = new Float32Array(capacity);
    }

    get size(): number {
        return this.currentSize;
    }

    clear(): void {
        this.currentSize = 0;
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

        if (this.currentSize + size > this.buffer.length) {
            overflow = true;
            this.readIndex = (this.readIndex + size) % this.buffer.length;
        } else {
            this.currentSize += size;
        }

        return overflow;
    }

    pull(output: Float32Array, startIndex: number, size: number): boolean {
        if (this.currentSize >= size) {
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

            this.currentSize -= size;

            return true;
        }

        return false;
    }
}

export default RingBuffer;
