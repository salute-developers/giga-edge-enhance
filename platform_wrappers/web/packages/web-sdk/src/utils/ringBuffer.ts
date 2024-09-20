class RingBuffer {
    capacity: number;

    size: number;

    _buffer: Float32Array;

    _write_index: number;

    _read_index: number;

    constructor(capacity: number) {
        // assert(capacity % 384 == 0);

        this.capacity = capacity;
        this.size = 0;

        this._buffer = new Float32Array(capacity);
        this._write_index = 0;
        this._read_index = 0;
    }

    push_128(input: Float32Array): boolean {
        return this._push(input, 128);
    }

    push_384(input: Float32Array): boolean {
        return this._push(input, 384);
    }

    pull_128(output: Float32Array, startIndex = 0): boolean {
        return this._pull(output, startIndex, 128);
    }

    pull_384(output: Float32Array, startIndex = 0): boolean {
        return this._pull(output, startIndex, 384);
    }

    _push(input: Float32Array, size: number): boolean {
        this._buffer.set(input, this._write_index);
        this._write_index = (this._write_index + size) % this.capacity;

        let overflow = false;

        if (this.size + size > this.capacity) {
            overflow = true;
            this._read_index = (this._read_index + size) % this.capacity;
        } else {
            this.size += size;
        }

        return overflow;
    }

    _pull(output: Float32Array, startIndex: number, size: number): boolean {
        if (this.size >= size) {
            this.size -= size;
            output.set(this._buffer.subarray(this._read_index, this._read_index + size), startIndex);
            this._read_index = (this._read_index + size) % this.capacity;

            return true;
        }

        return false;
    }

    clear(): void {
        this.size = 0;
        this._read_index = 0;
        this._write_index = 0;
    }
}

export default RingBuffer;
