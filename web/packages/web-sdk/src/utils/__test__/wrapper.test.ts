// packages/web-sdk/src/utils/__tests__/wrapper.test.ts
import { AbstractGigaEdgeSpenchWrapper } from '../wrapper';

/**
 * Concrete test implementation that exposes the protected members
 * of AbstractGigaEdgeSpenchWrapper for the test harness.
 */
class TestWrapper extends AbstractGigaEdgeSpenchWrapper {
    /* ──────────────────────────────────────────────────────────────────────
     *  Required abstract members.
     * ────────────────────────────────────────────────────────────────────── */
    private _heap = new Float32Array(1024);

    /* ──────────────────────────────────────────────────────────────────────
     *  Public façade – no business logic, just getters.
     * ────────────────────────────────────────────────────────────────────── */
    public get publicPipeline() {
        return this.pipeline;
    }

    public get publicDataStartIndex() {
        return this.dataStartIndex;
    }

    public get publicLbStartIndex() {
        return this.lbStartIndex;
    }

    public get publicCache() {
        return this.cache;
    }

    protected get heap(): Float32Array {
        return this._heap;
    }

    protected ensureScratchCapacity(frameSize: number, loopbackSize: number): void {
        if (this.dataStartIndex + frameSize > this._heap.length || this.lbStartIndex + loopbackSize > this._heap.length) {
            throw new RangeError('test scratch buffer too small');
        }
    }

    protected pop(): boolean {
        return this.pipeline.pop() !== undefined;
    }
}

describe('AbstractGigaEdgeSpenchWrapper', () => {
    let wrapper: TestWrapper;

    beforeEach(() => {
        wrapper = new TestWrapper();
    });

    /* --------------------------------------------------------------------- */
    /*  process() – no pipeline                                            */
    /* --------------------------------------------------------------------- */
    test('process returns a copy of the input frame when pipeline is empty', () => {
        const frame = new Float32Array([0.1, 0.2, 0.3]);
        const result = wrapper.process(frame);

        expect(result).toEqual(frame); // value equality
        expect(result).not.toBe(frame); // a new instance
    });

    /* --------------------------------------------------------------------- */
    /*  process() – CommonPipelineElement                                  */
    /* --------------------------------------------------------------------- */
    test('process correctly feeds a CommonPipelineElement', () => {
        const mockProcess = jest.fn((_one, _two) => 5); // returns processed sample count
        const commonEl: any = {
            process: mockProcess,
            resetContext: jest.fn(),
            delete: jest.fn(),
        };

        wrapper.publicPipeline.push(['common', commonEl]);

        const frame = new Float32Array([0.4, 0.5, 0.6, 0.7, 0.8]);

        const result = wrapper.process(frame);

        expect(mockProcess).toHaveBeenCalledTimes(1);
        expect(mockProcess).toHaveBeenCalledWith(wrapper.publicDataStartIndex, frame.length);

        expect(result.length).toBe(5);
        const expected = frame.subarray(0, 5);
        expect(result).toEqual(expected);
    });

    /* --------------------------------------------------------------------- */
    /*  process() – AECElement with loopback                               */
    /* --------------------------------------------------------------------- */
    test('process feeds an AECElement when loopback is provided', () => {
        const mockProcess = jest.fn((_one, _two, _three) => 4);
        const aecEl: any = {
            process: mockProcess,
            resetContext: jest.fn(),
            delete: jest.fn(),
        };

        wrapper.publicPipeline.push(['aec', aecEl]);

        const frame = new Float32Array([1, 2, 3, 4, 5, 6]);
        const loopback = new Float32Array([10, 20, 30, 40, 50, 60]);

        const result = wrapper.process(frame, loopback);

        expect(mockProcess).toHaveBeenCalledWith(
            wrapper.publicDataStartIndex,
            wrapper.publicLbStartIndex,
            frame.length,
        );
        expect(result.length).toBe(4);
    });

    test('process rejects a shorter loopback allocation', () => {
        const aecEl: any = {
            process: jest.fn(),
            resetContext: jest.fn(),
            delete: jest.fn(),
        };
        wrapper.publicPipeline.push(['aec', aecEl]);

        expect(() => wrapper.process(new Float32Array(5), new Float32Array(4))).toThrow(RangeError);
        expect(aecEl.process).not.toHaveBeenCalled();
    });

    /* --------------------------------------------------------------------- */
    /*  process() – break on zero output                                   */
    /* --------------------------------------------------------------------- */
    test('process stops iteration when an element returns 0', () => {
        const firstProcess = jest.fn((_one, _two) => 0); // stops the loop
        const secondProcess = jest.fn((_one, _two) => 3);

        const firstEl: any = {
            process: firstProcess,
            resetContext: jest.fn(),
            delete: jest.fn(),
        };
        const secondEl: any = {
            process: secondProcess,
            resetContext: jest.fn(),
            delete: jest.fn(),
        };

        wrapper.publicPipeline.push(['first', firstEl]);
        wrapper.publicPipeline.push(['second', secondEl]);

        const frame = new Float32Array([7, 8, 9, 10]);

        const result = wrapper.process(frame);

        expect(firstProcess).toHaveBeenCalledTimes(1);
        expect(secondProcess).not.toHaveBeenCalled();
        expect(result.length).toBe(0);
    });

    /* --------------------------------------------------------------------- */
    /*  clear() – empties the pipeline                                     */
    /* --------------------------------------------------------------------- */
    test('clear removes all elements via pop()', () => {
        const el: any = {
            process: jest.fn(),
            resetContext: jest.fn(),
            delete: jest.fn(),
        };
        wrapper.publicPipeline.push(['el', el]);

        expect(wrapper.publicPipeline.length).toBe(1);
        wrapper.clear();
        expect(wrapper.publicPipeline.length).toBe(0);
    });

    /* --------------------------------------------------------------------- */
    /*  resetContext() – forwards to each element                          */
    /* --------------------------------------------------------------------- */
    test('resetContext calls resetContext on all pipeline elements', () => {
        const el1: any = {
            process: jest.fn(),
            resetContext: jest.fn(),
            delete: jest.fn(),
        };
        const el2: any = {
            process: jest.fn(),
            resetContext: jest.fn(),
            delete: jest.fn(),
        };

        wrapper.publicPipeline.push(['e1', el1], ['e2', el2]);

        wrapper.resetContext();

        expect(el1.resetContext).toHaveBeenCalledTimes(1);
        expect(el2.resetContext).toHaveBeenCalledTimes(1);
    });

    /* --------------------------------------------------------------------- */
    /*  destroy() – deletes all elements                                    */
    /* --------------------------------------------------------------------- */
    test('destroy calls delete on all elements and empties pipeline', () => {
        const el1: any = {
            process: jest.fn(),
            resetContext: jest.fn(),
            delete: jest.fn(),
        };
        const el2: any = {
            process: jest.fn(),
            resetContext: jest.fn(),
            delete: jest.fn(),
        };

        wrapper.publicPipeline.push(['e1', el1], ['e2', el2]);

        wrapper.destroy();

        expect(el1.delete).toHaveBeenCalledTimes(1);
        expect(el2.delete).toHaveBeenCalledTimes(1);
        expect(wrapper.publicPipeline.length).toBe(0);
    });

    test('destroy clears and deletes cached elements', () => {
        const clear = jest.fn();
        const element: any = { delete: jest.fn(), resetContext: jest.fn() };
        wrapper.publicCache.set('cached', [clear, element]);

        wrapper.destroy();

        expect(clear).toHaveBeenCalledTimes(1);
        expect(element.delete).toHaveBeenCalledTimes(1);
        expect(wrapper.publicCache.size).toBe(0);
    });

    /* --------------------------------------------------------------------- */
    /*  tryGetFromCache – cache hit                                         */
    /* --------------------------------------------------------------------- */
    test('tryGetFromCache removes entry, clears and resets context', () => {
        const clearFn = jest.fn();
        const el: any = {
            process: jest.fn(),
            resetContext: jest.fn(),
            delete: jest.fn(),
        };
        wrapper.publicCache.set('key1', [clearFn, el]);

        const result = (wrapper as any).tryGetFromCache('key1');

        expect(result).toBe(el);
        expect(clearFn).toHaveBeenCalledTimes(1);
        expect(el.resetContext).toHaveBeenCalledTimes(1);
        expect(wrapper.publicCache.has('key1')).toBe(false);
    });

    /* --------------------------------------------------------------------- */
    /*  tryGetFromCache – cache miss                                        */
    /* --------------------------------------------------------------------- */
    test('tryGetFromCache returns undefined when key not in cache', () => {
        const res = (wrapper as any).tryGetFromCache('nonexistent');
        expect(res).toBeUndefined();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
