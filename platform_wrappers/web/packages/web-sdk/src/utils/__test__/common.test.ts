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
});

describe('default noise cancellation models', () => {
    const models = {
        noiseCancellation: {
            lite8k: new URL('https://example.com/fast8.spench'),
            full48k: new URL('https://example.com/quality48.spench'),
        },
    };

    it('uses the bundled fast model for 8 kHz input', () => {
        expect(getDefaultNoiseCancellationModel(models, 8000)).toBe('lite8k');
    });

    it('uses the bundled quality model above 8 kHz', () => {
        expect(getDefaultNoiseCancellationModel(models, 8001)).toBe('full48k');
    });
});
