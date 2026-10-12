// packages/web-sdk/src/workers/worker/__tests__/wrapper.test.ts
/**
 * @jest-environment node
 */
import { GigaEdgeSpenchWrapper, GigaEdgeSpenchDspWrapper } from '../wrapper';
import { LoadError } from '../loader';
import getHashCode from '../../../utils/hash';
import { getDefaultAECConfig, getDefaultNormalizationConfig } from '../../../utils/constants';
import { Settings } from '../../../utils/common';

jest.mock('../../../../generated/wasm/gigaedge-spench', () => {
    const bridge = {
        _malloc: jest.fn((size: number) => size / Float32Array.BYTES_PER_ELEMENT),
        _free: jest.fn(),
        HEAPF32: new Float32Array(1024),
        HEAPU8: new Uint8Array(1024),
        DenoisePipelineElement: jest.fn(() => dummyElement()),
        PackedModelReader: jest.fn(() => dummyPackedModelReader()),
    };

    return () => bridge;
}, { virtual: true });

jest.mock('../../../../generated/wasm/gigaedge-spench.dsp', () => {
    const bridge = {
        _malloc: jest.fn((size: number) => size / Float32Array.BYTES_PER_ELEMENT),
        _free: jest.fn(),
        HEAPF32: new Float32Array(1024),
        AudioProcessPipelineElement: jest.fn(() => dummyElement()),
        EchoCancelationElement: jest.fn(() => dummyElement()),
    };

    return () => bridge;
}, { virtual: true });

jest.mock('../../../utils/constants', () => ({
    FEATURE_SWITCH_CACHE_MILLISECONDS: 0, // fast expiry for tests
    getDefaultAECConfig: jest.fn(() => 'DEFAULT_AEC'),
    getDefaultNormalizationConfig: jest.fn(() => 'DEFAULT_NORMALIZER'),
    HEAP_SIZE: 512,
}));

jest.mock('../../../utils/hash', () => jest.fn((s: string) => `hash(${s})`));

function dummyElement() {
    return {
        process: jest.fn((..._: any[]) => 5),
        resetContext: jest.fn(),
        delete: jest.fn(),
    };
}

function dummyPackedModelReader() {
    return {
        delete: jest.fn(),
    };
}

class DummyLogger {
    debug = jest.fn();

    error = jest.fn();
}

class DummyLoader {
    async loadWasm() {
        return new ArrayBuffer(4);
    }

    async loadDSPWasm() {
        return new ArrayBuffer(4);
    }

    async loadNoiseCancellationModel(_type: string) {
        return new ArrayBuffer(8);
    }

}

describe('GigaEdgeSpenchWrapper', () => {
    let wrapper: GigaEdgeSpenchWrapper;
    const loader = new DummyLoader() as any;
    const logger = new DummyLogger() as any;
    const heavyStart = jest.fn();
    const heavyEnd = jest.fn();

    beforeEach(async () => {
        heavyStart.mockClear();
        heavyEnd.mockClear();
        wrapper = (await GigaEdgeSpenchWrapper.create({
            sampleRate: 48000,
            logger,
            loader,
            onHeavyLoadStart: heavyStart,
            onHeavyLoadEnd: heavyEnd,
        })) as GigaEdgeSpenchWrapper;
    });

    test('create succeeds with wasm', async () => {
        const instance = await GigaEdgeSpenchWrapper.create({
            sampleRate: 48000,
            logger,
            loader,
            onHeavyLoadStart: heavyStart,
            onHeavyLoadEnd: heavyEnd,
        });
        expect(instance).toBeInstanceOf(GigaEdgeSpenchWrapper);
    });

    test('create fails with LoadError', async () => {
        const badLoader = new DummyLoader() as any;
        badLoader.loadWasm = jest.fn(() => Promise.resolve(new LoadError('bad')));
        const result = await GigaEdgeSpenchWrapper.create({
            sampleRate: 48000,
            logger,
            loader: badLoader,
            onHeavyLoadStart: heavyStart,
            onHeavyLoadEnd: heavyEnd,
        });
        expect(result).toBeInstanceOf(LoadError);
    });

    test('pop removes element and schedules cache eviction', async () => {
        const elem = { delete: jest.fn(), resetContext: jest.fn() };
        (wrapper as any).pipeline.push(['x', elem as any]);

        const popped = wrapper.pop();
        expect(popped).toBe(true);
        expect((wrapper as any).pipeline.length).toBe(0);
        expect((wrapper as any).cache.has('x')).toBe(true);
        // cache should contain the element
        const cacheEntry = (wrapper as any).cache.get('x');
        expect(cacheEntry).toBeDefined();
        const [, cachedEl] = cacheEntry!;
        expect(cachedEl).toBe(elem);

        await new Promise((r) => setTimeout(r, 0));
        // cleared after timeout
        expect(elem.delete).toHaveBeenCalledTimes(1);
        expect((wrapper as any).cache.has('x')).toBe(false);
    });

    test('destroy frees memory and deletes pipeline elements', () => {
        const elem = { delete: jest.fn(), resetContext: jest.fn() };
        (wrapper as any).pipeline.push(['x', elem as any]);

        wrapper.destroy();
        expect((wrapper as any).pipeline.length).toBe(0);
        expect(elem.delete).toHaveBeenCalled();
        // bridge._free should have been called for dataPtr
        expect((wrapper as any).bridge._free).toHaveBeenCalledWith(expect.any(Number));
    });

    test('grows the input allocation for oversized chunks', () => {
        const elem = dummyElement();
        elem.process.mockReturnValue(600);
        (wrapper as any).pipeline.push(['x', elem]);

        wrapper.process(new Float32Array(600));

        expect((wrapper as any).dataCapacity).toBe(600);
        expect((wrapper as any).bridge._malloc).toHaveBeenCalledWith(600 * Float32Array.BYTES_PER_ELEMENT);
    });

    /* ---------------------- Noise Canceller ---------------------- */

    test('addNoiseCanceller loads model, creates element, and caches it', async () => {
        expect(heavyStart).toHaveBeenCalledTimes(1);
        expect(heavyEnd).toHaveBeenCalledTimes(1);
        heavyStart.mockClear();
        heavyEnd.mockClear();
        const res = await wrapper.addNoiseCanceller('tiny' as any);
        expect(res).toBeUndefined();
        expect(heavyStart).toHaveBeenCalledTimes(1);
        expect(heavyEnd).toHaveBeenCalledTimes(1);
        expect((wrapper as any).pipeline.length).toBe(1);
        expect((wrapper as any).pipeline[0][0]).toBe('denoiser-tiny');

        // second call should hit cache
        heavyStart.mockClear();
        heavyEnd.mockClear();
        wrapper.clear();
        const res2 = await wrapper.addNoiseCanceller('tiny' as any);
        expect(res2).toBeUndefined();
        expect(heavyStart).not.toHaveBeenCalled();
        expect(heavyEnd).not.toHaveBeenCalled();
    });

    test('addNoiseCanceller propagates Loader errors', async () => {
        const badLoader = new DummyLoader() as any;
        badLoader.loadNoiseCancellationModel = jest.fn(() => Promise.resolve(new LoadError('bad model')));
        const res = await GigaEdgeSpenchWrapper.create({
            sampleRate: 48000,
            logger,
            loader: badLoader,
            onHeavyLoadStart: heavyStart,
            onHeavyLoadEnd: heavyEnd,
        }).then((w) => (w as any).addNoiseCanceller('tiny'));
        expect(res).toBeInstanceOf(LoadError);
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});

describe('GigaEdgeSpenchDspWrapper', () => {
    let wrapper: GigaEdgeSpenchDspWrapper;
    const settings = {} as Settings;
    const loader = new DummyLoader() as any;
    const logger = new DummyLogger() as any;

    beforeEach(async () => {
        wrapper = (await GigaEdgeSpenchDspWrapper.create({
            sampleRate: 48000,
            logger,
            loader,
            settings,
        })) as GigaEdgeSpenchDspWrapper;
    });

    test('create succeeds with DSP wasm', async () => {
        const instance = await GigaEdgeSpenchDspWrapper.create({
            sampleRate: 48000,
            logger,
            loader,
            settings,
        });
        expect(instance).toBeInstanceOf(GigaEdgeSpenchDspWrapper);
    });

    test('create fails with LoadError', async () => {
        const badLoader = new DummyLoader() as any;
        badLoader.loadDSPWasm = jest.fn(() => Promise.resolve(new LoadError('bad')));
        const result = await GigaEdgeSpenchDspWrapper.create({
            sampleRate: 48000,
            logger,
            loader: badLoader,
            settings,
        });
        expect(result).toBeInstanceOf(LoadError);
    });

    test('addNormalizer creates default element', () => {
        const res = wrapper.addNormalizer();
        expect(res).toBeUndefined();
        expect(getDefaultNormalizationConfig).toHaveBeenCalledWith(48000, settings);
        expect((wrapper as any).pipeline.length).toBe(1);
        expect((wrapper as any).pipeline[0][0]).toBe('normalizer-default');
    });

    test('addNormalizer creates element', () => {
        const config = 'custom';
        const res = wrapper.addNormalizer(config);
        expect(res).toBeUndefined();
        expect(getDefaultNormalizationConfig).not.toHaveBeenCalled();
        expect(getHashCode).toHaveBeenCalledWith(config);
        expect((wrapper as any).pipeline.length).toBe(1);
        expect((wrapper as any).pipeline[0][0]).toBe(`normalizer-hash(${config})`);
    });

    test('addEchoCanceller creates element with custom config', () => {
        const custom = 'CUSTOM';
        wrapper.addEchoCanceller(custom);
        expect(getHashCode).toHaveBeenCalledWith(custom);
        expect(getDefaultAECConfig).not.toHaveBeenCalled();
        expect((wrapper as any).pipeline.length).toBe(1);
        expect((wrapper as any).pipeline[0][0]).toBe('aec-hash(CUSTOM)');
    });

    test('pop removes element and schedules cache eviction', async () => {
        const elem = { delete: jest.fn(), resetContext: jest.fn() };
        (wrapper as any).pipeline.push(['x', elem as any]);

        const popped = wrapper.pop();
        expect(popped).toBe(true);
        expect((wrapper as any).pipeline.length).toBe(0);
        expect((wrapper as any).cache.has('x')).toBe(true);
        // cache should contain the element
        const cacheEntry = (wrapper as any).cache.get('x');
        expect(cacheEntry).toBeDefined();
        const [, cachedEl] = cacheEntry!;
        expect(cachedEl).toBe(elem);

        await new Promise((r) => setTimeout(r, 0));
        // cleared after timeout
        expect(elem.delete).toHaveBeenCalledTimes(1);
        expect((wrapper as any).cache.has('x')).toBe(false);
    });

    test('destroy frees memory and deletes pipeline elements', () => {
        const elem = { delete: jest.fn(), resetContext: jest.fn() };
        (wrapper as any).pipeline.push(['z', elem as any]);

        wrapper.destroy();
        expect((wrapper as any).pipeline.length).toBe(0);
        expect(elem.delete).toHaveBeenCalled();
        expect((wrapper as any).bridge._free).toHaveBeenCalledWith(expect.any(Number));
    });

    test('grows input and loopback allocations independently', () => {
        const elem = dummyElement();
        (wrapper as any).pipeline.push(['aec', elem]);

        wrapper.process(new Float32Array(600), new Float32Array(700));

        expect((wrapper as any).dataCapacity).toBe(600);
        expect((wrapper as any).loopbackCapacity).toBe(700);
        expect((wrapper as any).bridge._malloc).toHaveBeenCalledWith(600 * Float32Array.BYTES_PER_ELEMENT);
        expect((wrapper as any).bridge._malloc).toHaveBeenCalledWith(700 * Float32Array.BYTES_PER_ELEMENT);
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
