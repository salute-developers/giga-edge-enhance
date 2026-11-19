/* eslint-disable no-var */
import { GigaEdgeEnhanceDspWorkletWrapper } from '../wrapper';
import { Logger } from '../../../utils/logger';
import { Settings } from '../../../utils/common';
import { getDefaultNormalizationConfig } from '../../../utils/constants';

// ---------------------------------------------------------------------------
// Mocks
// ---------------------------------------------------------------------------
var mockHeap: Float32Array;
var mockAudioProcessElement: jest.Mock;
var mockAECElement: jest.Mock;
var mockMalloc: jest.Mock; // lbPtr
var mockFree: jest.Mock;
// 1. Mock constants
jest.mock('../../../utils/constants', () => ({
    HEAP_SIZE: 64, // 64 floats => 256 bytes
    FEATURE_SWITCH_CACHE_MILLISECONDS: 0, // 0.5s
    getDefaultAECConfig: jest.fn(() => 'defaultAECConfig'),
    getDefaultNormalizationConfig: jest.fn(() => 'defaultNormalizationConfig'),
}));

// 2. Mock hash function (deterministic)
jest.mock('../../../utils/hash', () => () => 'mockedHash');

jest.mock('../../../../generated/wasm/gigaedge-enhance.dsp', () => {
    return {
        __esModule: true,
        default: () => ({
            _malloc: (mockMalloc = jest
                .fn()
                .mockReturnValueOnce(1000) // dataPtr
                .mockReturnValueOnce(2000)), // lbPtr),
            _free: (mockFree = jest.fn()),
            HEAPF32: (mockHeap = new Float32Array(1024)),
            AudioProcessPipelineElement: (mockAudioProcessElement = jest.fn((config: string) => ({
                config,
                process: jest.fn((_srcPtr: number, _chunkSize: number) => _chunkSize), // echoes input
                resetContext: jest.fn(),
                delete: jest.fn(),
            }))),
            EchoCancelationElement: (mockAECElement = jest.fn((config: string) => ({
                config,
                process: jest.fn((_srcPtr: number, _lbPtr: number, _chunkSize: number) => _chunkSize),
                resetContext: jest.fn(),
                delete: jest.fn(),
            }))),
        }),
        MainModule: () => ({}), // type placeholder
    };
}, { virtual: true });

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------

/**
 * Small utility to expose protected members of GigaEdgeEnhanceDspWorkletWrapper
 * for assertion purposes.  The wrapper itself is a thin wrapper, so
 * we don't need a full subclass – we just use a type cast to `any`.
 */
function asAny<T>(obj: T): any {
    return obj as any;
}

// ---------------------------------------------------------------------------
// Test setup
// ---------------------------------------------------------------------------

const mockLogger = {
    debug: jest.fn(),
    error: jest.fn(),
} as any as Logger;

beforeEach(() => {
    jest.clearAllMocks();
    // Fake currentTime global (AudioWorklet)
    (globalThis as any).currentTime = 0;
});

describe('GigaEdgeEnhanceDspWorkletWrapper', () => {
    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------
    test('constructor allocates heap and stores bridge', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        // _malloc should be called twice with HEAP_SIZE * 4
        const expectedBytes = 64 * Float32Array.BYTES_PER_ELEMENT; // 256
        expect(mockMalloc).toHaveBeenCalledTimes(2);
        expect(mockMalloc).toHaveBeenNthCalledWith(1, expectedBytes);
        expect(mockMalloc).toHaveBeenNthCalledWith(2, expectedBytes);

        // Pointers stored
        expect((wrapper as any).dataPtr).toBe(1000);
        expect((wrapper as any).lbPtr).toBe(2000);

        // Heap is the mock heap
        expect(asAny(wrapper).heap).toBe(mockHeap);
    });

    // -----------------------------------------------------------------------
    // addNormalizer
    // -----------------------------------------------------------------------
    test('addNormalizer creates a new normalizer when not cached (default)', () => {
        const wasm = new ArrayBuffer(8);
        const settings = {} as Settings;
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, settings);

        wrapper.addNormalizer(); // no config

        // Pipeline has one element
        const { pipeline } = asAny(wrapper);
        expect(pipeline.length).toBe(1);
        const [key, element] = pipeline[0];
        expect(key).toBe('normalizer-default');
        expect(element.config).toBe('defaultNormalizationConfig');

        // Ensure the constructor was called with default config
        expect(mockAudioProcessElement).toHaveBeenCalledWith('defaultNormalizationConfig');
        expect(getDefaultNormalizationConfig).toHaveBeenCalledWith(48000, settings);
    });

    test('addNormalizer creates a new normalizer when not cached', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        wrapper.addNormalizer('aba');

        // Pipeline has one element
        const { pipeline } = asAny(wrapper);
        expect(pipeline.length).toBe(1);
        const [key, element] = pipeline[0];
        expect(key).toBe('normalizer-mockedHash');
        expect(element.config).toBe('aba');

        // Ensure the constructor was called with default config
        expect(mockAudioProcessElement).toHaveBeenCalledWith('aba');
    });

    test('addNormalizer re‑uses a cached normalizer', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        // Pre‑populate the cache
        const cachedElement = { resetContext: jest.fn(), delete: jest.fn() };
        const clearFn = jest.fn();
        asAny(wrapper).cache.set('normalizer-default', [clearFn, cachedElement]);

        wrapper.addNormalizer(); // should reuse cached

        expect(mockAudioProcessElement).not.toHaveBeenCalled();
        expect(cachedElement.resetContext).toHaveBeenCalledTimes(1);
        expect(clearFn).toHaveBeenCalledTimes(1); // clear timeout after acquire cache

        const { pipeline } = asAny(wrapper);
        expect(pipeline[0][1]).toBe(cachedElement);
    });

    // -----------------------------------------------------------------------
    // addEchoCanceller
    // -----------------------------------------------------------------------
    test('addEchoCanceller creates a new AEC when not cached (default)', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        wrapper.addEchoCanceller(); // no config

        const { pipeline } = asAny(wrapper);
        expect(pipeline.length).toBe(1);
        const [key, element] = pipeline[0];
        expect(key).toBe('aec-default');
        expect(element.config).toBe('defaultAECConfig');
        expect(mockAECElement).toHaveBeenCalledWith('defaultAECConfig');
    });

    test('addEchoCanceller creates a new AEC when not cached', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        wrapper.addEchoCanceller('aba');

        const { pipeline } = asAny(wrapper);
        expect(pipeline.length).toBe(1);
        const [key, element] = pipeline[0];
        expect(key).toBe('aec-mockedHash');
        expect(element.config).toBe('aba');
        expect(mockAECElement).toHaveBeenCalledWith('aba');
    });

    // -----------------------------------------------------------------------
    // pop
    // -----------------------------------------------------------------------
    test('pop removes the last element, stores it in cache and timeout', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        // Insert a dummy element manually
        const dummy = { delete: jest.fn(), resetContext: jest.fn(), process: jest.fn() };
        const { pipeline } = asAny(wrapper);
        pipeline.push(['dummy-key', dummy]);

        // Set a deterministic currentTime
        (globalThis as any).currentTime = 1.234;

        const result = wrapper.pop();
        expect(result).toBe(true);

        // Pipeline should be empty
        expect(pipeline.length).toBe(0);

        // Cache should contain the element
        const { cache } = asAny(wrapper);
        expect(cache.has('dummy-key')).toBe(true);
        const [clearFn, cachedEl] = cache.get('dummy-key')!;
        expect(cachedEl).toBe(dummy);

        // Timeout should be set
        const { timeouts } = asAny(wrapper);
        expect(timeouts.get('dummy-key')).toBe(1.234);

        // clearFn removes from timeouts
        clearFn();
        expect(timeouts.has('dummy-key')).toBe(false);
    });

    test('pop returns false when pipeline is empty', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);
        expect(wrapper.pop()).toBe(false);
    });

    // -----------------------------------------------------------------------
    // tick
    // -----------------------------------------------------------------------
    test('tick removes stale cache entries and deletes elements', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        // Add a dummy element and pop it
        const dummy = { delete: jest.fn(), resetContext: jest.fn(), process: jest.fn() };
        const { pipeline } = asAny(wrapper);
        pipeline.push(['dummy-key', dummy]);

        (globalThis as any).currentTime = 0;
        wrapper.pop(); // sets timeout to 0

        // Advance time beyond FEATURE_SWITCH_CACHE_MILLISECONDS (500ms)
        (globalThis as any).currentTime = 0.6; // 600ms

        wrapper.tick();

        // Cache and timeout removed
        expect(asAny(wrapper).cache.has('dummy-key')).toBe(false);
        expect(asAny(wrapper).timeouts.has('dummy-key')).toBe(false);

        // Element delete should have been called
        expect(dummy.delete).toHaveBeenCalledTimes(1);
    });

    // -----------------------------------------------------------------------
    // destroy
    // -----------------------------------------------------------------------
    test('destroy frees heap and deletes pipeline elements', () => {
        const wasm = new ArrayBuffer(8);
        const wrapper = new GigaEdgeEnhanceDspWorkletWrapper(wasm, 48000, mockLogger, {} as Settings);

        // Add two dummy elements
        const el1 = { delete: jest.fn(), resetContext: jest.fn(), process: jest.fn() };
        const el2 = { delete: jest.fn(), resetContext: jest.fn(), process: jest.fn() };
        const { pipeline } = asAny(wrapper);
        pipeline.push(['el1', el1]);
        pipeline.push(['el2', el2]);

        wrapper.destroy();

        // All elements should be deleted
        expect(el1.delete).toHaveBeenCalledTimes(1);
        expect(el2.delete).toHaveBeenCalledTimes(1);

        // Heap freed
        expect(mockFree).toHaveBeenCalledWith((wrapper as any).dataPtr);
        expect(mockFree).toHaveBeenCalledWith((wrapper as any).lbPtr);

        // Pipeline empty
        expect(pipeline.length).toBe(0);
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
