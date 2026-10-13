import { copyArrayBuffer, getDefaultNoiseCancellationModel, getSettingsWithDefaults } from '../common';
import { getOutputBufferSeconds, getProcessingChunkSizeSeconds, TRANSPORT_SIZE_SECONDS } from '../constants';

describe('copyArrayBuffer', () => {
    it('copies an ArrayBuffer', () => {
        const first = new Float32Array([1, 2.3, 4, 5.67, 8, 9, 0]);
        const copied = copyArrayBuffer(first.buffer);
        const second = new Float32Array(copied);

        expect(first).not.toBe(second);
        expect(first).toEqual(second);
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});

describe('chunk size settings', () => {
    it('defaults max processing chunk size to 16ms', () => {
        expect(getSettingsWithDefaults().maxProcessingChunkSeconds).toBe(0.016);
    });

    it('derives processing chunk size from max chunk and min latency', () => {
        const settings = getSettingsWithDefaults({
            minLatency: 0.05,
            maxProcessingChunkSeconds: 0.064,
        });

        expect(getProcessingChunkSizeSeconds(settings)).toBe(0.048);
    });

    it('never returns processing chunks below 16ms', () => {
        const settings = getSettingsWithDefaults({
            minLatency: 0.008,
            maxProcessingChunkSeconds: 0.008,
        });

        expect(getProcessingChunkSizeSeconds(settings)).toBe(0.016);
    });

    it('uses a fixed 16ms transfer chunk size', () => {
        expect(TRANSPORT_SIZE_SECONDS).toBe(0.016);
    });

    it('derives output buffer size from low performance settings', () => {
        const settings = getSettingsWithDefaults({
            minLatency: 0.064,
            latencyIncreaseStep: 0.032,
            lowPerformanceMaxRefillTimes: 4,
        });

        expect(getOutputBufferSeconds(settings)).toBe(0.16);
    });

    it.each([
        ['fetchRetries', -1],
        ['fetchRetries', 1.5],
        ['fetchRetries', 11],
        ['minLatency', Number.NaN],
        ['latencyIncreaseStep', Number.POSITIVE_INFINITY],
        ['maxProcessingChunkSeconds', 1.01],
        ['lowPerformanceWindowSeconds', 61],
        ['lowPerformanceMaxRefillTimes', 33],
        ['lowPerformanceMaxRefillTimeFactor', -1],
    ])('rejects invalid numeric setting %s=%s', (name, value) => {
        expect(() => getSettingsWithDefaults({ [name]: value })).toThrow(RangeError);
    });
});

describe('default noise cancellation models', () => {
    const models = {
        noiseCancellation: {
            fast8: new URL('https://example.com/fast8.ges'),
            quality48: new URL('https://example.com/quality48.ges'),
        },
    };

    it('uses the bundled fast model for 8 kHz input', () => {
        expect(getDefaultNoiseCancellationModel(models, 8000)).toBe('fast8');
    });

    it('uses the bundled quality model above 8 kHz', () => {
        expect(getDefaultNoiseCancellationModel(models, 8001)).toBe('quality48');
    });

    it.each([
        ['quality48', 'quality48'],
        ['custom', 'custom'],
    ] as const)('selects an available %s model', (model, expected) => {
        expect(getDefaultNoiseCancellationModel({ noiseCancellation: { [model]: new ArrayBuffer(1) } }, 48_000)).toBe(
            expected,
        );
    });

    it('prefers the bundled 48 kHz model over a custom model', () => {
        expect(
            getDefaultNoiseCancellationModel(
                {
                    noiseCancellation: {
                        custom: new ArrayBuffer(1),
                        quality48: new ArrayBuffer(1),
                    },
                },
                48_000,
            ),
        ).toBe('quality48');
    });
});
