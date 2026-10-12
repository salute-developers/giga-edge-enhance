interface AbstractPipelineElement {
    resetContext(): void;
    delete(): void;
}

interface CommonPipelineElement extends AbstractPipelineElement {
    process(srcPtr: number, chunkSize: number): number;
}

interface AECElement extends AbstractPipelineElement {
    process(srcPtr: number, lbPtr: number, chunkSize: number): number;
}

type PipelineElement = CommonPipelineElement | AECElement;

type Element = [cacheKey: string, PipelineElement];

export abstract class AbstractGigaEdgeSpenchWrapper {
    protected readonly pipeline: Element[] = [];

    protected dataPtr = 0;

    protected lbPtr = 0;

    protected readonly cache: Map<string, [() => void, PipelineElement]> = new Map();

    protected get dataStartIndex() {
        return this.dataPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    protected get lbStartIndex() {
        return this.lbPtr / Float32Array.BYTES_PER_ELEMENT;
    }

    protected abstract get heap(): Float32Array;

    protected abstract ensureScratchCapacity(frameSize: number, loopbackSize: number): void;

    clear() {
        while (this.pop());
    }

    process(frame: Float32Array, loopback?: Float32Array): Float32Array {
        if (!this.pipeline.length) {
            return new Float32Array(frame.subarray());
        }

        if (loopback && loopback.length < frame.length) {
            throw new RangeError('Loopback frame must contain at least as many samples as the input frame');
        }
        this.ensureScratchCapacity(frame.length, loopback?.length || 0);
        this.heap.set(frame, this.dataStartIndex);

        if (loopback) {
            this.heap.set(loopback, this.lbStartIndex);
        }

        let samplesProcessed = frame.length;

        for (const [, el] of this.pipeline) {
            if (el.process.length === 2) {
                samplesProcessed = (el as CommonPipelineElement).process(this.dataPtr, samplesProcessed);
            } else if (el.process.length === 3 && loopback) {
                samplesProcessed = (el as AECElement).process(this.dataPtr, this.lbPtr, samplesProcessed);
            }

            if (!samplesProcessed) break;
        }

        return new Float32Array(this.heap.subarray(this.dataStartIndex, this.dataStartIndex + samplesProcessed));
    }

    resetContext() {
        for (const [, el] of this.pipeline) {
            el.resetContext();
        }
    }

    destroy(): void {
        let current;
        while ((current = this.pipeline.pop())) {
            const [, element] = current;
            element.delete();
        }
        for (const [clear, element] of this.cache.values()) {
            clear();
            element.delete();
        }
        this.cache.clear();
    }

    protected tryGetFromCache(key: string): PipelineElement | undefined {
        const res = this.cache.get(key);

        if (!res) return;
        this.cache.delete(key);
        const [clear, element] = res;
        clear();
        element.resetContext();

        return element;
    }

    protected abstract pop(): boolean;
}
