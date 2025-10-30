import Loader, { LoadError } from '../loader';
import { downloadFileAsBuffer, retry } from '../../../utils/common';

jest.mock('../../../utils/common', () => ({
    downloadFileAsBuffer: jest.fn(),
    retry: jest.fn((fn: () => Promise<any>) => fn()),
}));

describe('Loader', () => {
    const wasmUrl = 'https://example.com/wasm';
    const wasmDspUrl = 'https://example.com/wasm-dsp';
    const dataUrl = 'https://example.com/data';

    const resourceUrls = {
        wasmUrl,
        wasmDspUrl,
        dataUrl,
    } as const;

    const telemetryMock = {
        sendMetric: jest.fn(),
    } as any;

    const settings = {
        fetchRetries: 3,
    } as any;

    const buffer = new ArrayBuffer(8);
    const time = 123;

    let loader: Loader;

    beforeEach(() => {
        jest.clearAllMocks();
        loader = new Loader(resourceUrls, telemetryMock, settings);
    });

    it('loads wasm and reports metric', async () => {
        (downloadFileAsBuffer as jest.Mock).mockResolvedValue([buffer, time]);

        const result = await loader.loadWasm();

        expect(retry).toHaveBeenCalledWith(expect.any(Function), settings.fetchRetries);
        expect(downloadFileAsBuffer).toHaveBeenCalledWith(wasmUrl);
        expect(telemetryMock.sendMetric).toHaveBeenCalledWith('wasmDownloadTime', time);
        expect(result).toBe(buffer);
    });

    it('loads DSP wasm without caching and reports metric each call', async () => {
        (downloadFileAsBuffer as jest.Mock).mockResolvedValue([buffer, time]);

        const first = await loader.loadDSPWasm();
        const second = await loader.loadDSPWasm();

        expect(retry).toHaveBeenCalledTimes(2);
        expect(downloadFileAsBuffer).toHaveBeenCalledTimes(2);
        expect(downloadFileAsBuffer).toHaveBeenNthCalledWith(1, wasmDspUrl);
        expect(downloadFileAsBuffer).toHaveBeenNthCalledWith(2, wasmDspUrl);
        expect(first).toBe(buffer);
        expect(second).toBe(buffer);
    });

    it('loads models data and caches the promise', async () => {
        (downloadFileAsBuffer as jest.Mock).mockResolvedValue([buffer, time]);

        const first = await loader.loadModelsData();
        const second = await loader.loadModelsData();

        expect(retry).toHaveBeenCalledTimes(1);
        expect(downloadFileAsBuffer).toHaveBeenCalledTimes(1);
        expect(downloadFileAsBuffer).toHaveBeenCalledWith(dataUrl);
        expect(first).toBe(buffer);
        expect(second).toBe(buffer);
    });

    it('returns a LoadError when download fails and does not report metric', async () => {
        const err = new Error('network');
        (downloadFileAsBuffer as jest.Mock).mockRejectedValue(err);

        const result = await loader.loadWasm();

        expect(retry).toHaveBeenCalledTimes(1);
        expect(downloadFileAsBuffer).toHaveBeenCalledWith(wasmUrl);
        expect(telemetryMock.sendMetric).not.toHaveBeenCalled();
        expect(result).toBeInstanceOf(LoadError);
        expect((result as LoadError).message).toContain('network');
    });
});
