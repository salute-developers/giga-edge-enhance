import { SELF_WORKER_MESSAGES } from '../utils/ipc';
import GigaDenoiserWorker, { getTransferableModelRepresentation } from '../worker';
import { Logger } from '../utils/logger';
import { Telemetry } from '../utils/telemetry';
import { Settings, ModelsTmpl } from '../utils/common';
import { WASM_DSP_URL, WASM_URL } from '../utils/resources';

jest.mock('../utils/jsResources', () => ({
    WORKER_URL: '123',
}));

class MockWorker extends EventTarget {
    readonly receivedMessages: { msg: any; transfer?: any }[] = [];

    destroyed = false;

    constructor(public readonly url: string) {
        super();
    }

    postMessage(msg: any, transfer?: any) {
        this.receivedMessages.push({ msg, transfer });
    }

    addEventListener(type: string, listener: any) {
        super.addEventListener(type, listener);
    }

    removeEventListener(type: string, listener: any) {
        super.removeEventListener(type, listener);
    }

    close() {}

    destroy() {
        this.destroyed = true;
    }
}

jest.mock('../utils/resources', () => {
    // eslint-disable-next-line global-require
    (global as any).MessageChannel = require('node:worker_threads').MessageChannel;
    (global as any).Worker = class extends EventTarget {
        readonly receivedMessages: { msg: any; transfer?: any }[] = [];

        destroyed = false;

        constructor(public readonly url: string) {
            super();
        }

        postMessage(msg: any, transfer?: any) {
            this.receivedMessages.push({ msg, transfer });
        }

        addEventListener(type: string, listener: any) {
            super.addEventListener(type, listener);
        }

        removeEventListener(type: string, listener: any) {
            super.removeEventListener(type, listener);
        }

        close() {}

        destroy() {
            this.destroyed = true;
        }
    };

    return {
        WASM_DSP_URL: '123',
        WASM_URL: '123',
    };
});

/** Minimal Settings object – enough for the worker constructor. */
const dummySettings = {
    fetchRetries: 1,
    lowPerformanceWindowSeconds: 1,
    lowPerformanceMaxRefillTimes: 1,
    lowPerformanceMaxRefillTimeFactor: 1,
    minLatency: 0,
    latencyIncreaseStep: 0,
    maxProcessingChunkSeconds: 0.016,
    sharedMemoryEnabled: true,
    forceSharedMemory: false,
    dspOnAudioThread: false,
    useVadNormalization: false,
    unstable_reverseProcessingOrder: false,
    unstable_customNormalizationConfig: undefined,
    unstable_customAECConfig: undefined,
} as Settings;

/* -------------------------------------------------------------------------- */
/*  Mock Worker                                                               */
/* -------------------------------------------------------------------------- */

// let OriginalWorker: any;

// beforeAll(() => {
//     // Replace the global Worker with our mock implementation.
//     OriginalWorker = (global as any).Worker;
//     (global as any).Worker = MockWorker;
// });

// afterAll(() => {
//     // Restore the original Worker implementation.
//     (global as any).Worker = OriginalWorker;
// });

/* -------------------------------------------------------------------------- */
/*  Tests for getTransferableModelRepresentation                              */
/* -------------------------------------------------------------------------- */

describe('getTransferableModelRepresentation', () => {
    test('returns [undefined, []] when raw models are undefined', () => {
        const [models, trans] = getTransferableModelRepresentation(undefined);
        expect(models).toBeUndefined();
        expect(trans).toEqual([]);
    });

    test('converts URLs and copies caller-owned ArrayBuffers before transfer', () => {
        const raw = {
            noiseCancellation: {
                fast24: new ArrayBuffer(4),
                quality24: new URL('https://example.com/quality24.gdn'),
                custom: 'https://example.com/custom.gdn',
            },
        } as unknown as ModelsTmpl<string | ArrayBuffer>;

        const [models, trans] = getTransferableModelRepresentation(raw);

        // Transferables should contain the single ArrayBuffer.
        expect(trans).toHaveLength(1);
        expect(trans[0]).toBeInstanceOf(ArrayBuffer);
        expect(trans[0]).not.toBe((raw as any).noiseCancellation.fast24);
        expect(trans[0]).toEqual((raw as any).noiseCancellation.fast24);

        // Models map should have href string for URL and keep strings untouched.
        expect(models).toMatchObject({
            noiseCancellation: {
                fast24: trans[0],
                quality24: 'https://example.com/quality24.gdn',
                custom: 'https://example.com/custom.gdn',
            },
        });
    });

});

/* -------------------------------------------------------------------------- */
/*  Tests for GigaDenoiserWorker                                                    */
/* -------------------------------------------------------------------------- */

describe('GigaDenoiserWorker', () => {
    const dummyLogger = { debug: jest.fn(), verbosity: 'verbose' } as any as Logger;
    const dummyTelemetry = {} as Telemetry;

    test('constructor posts INIT message with the correct resources', () => {
        const rawModels = {
            noiseCancellation: {
                fast24: new ArrayBuffer(4),
                quality24: new URL('https://example.com/model.gdn'),
            },
        } as unknown as ModelsTmpl<string | ArrayBuffer>;

        const worker = new GigaDenoiserWorker(dummyLogger, dummyTelemetry, dummySettings, rawModels);

        // The mock Worker instance lives on the `worker` field of the class.
        const mockWorker = worker as unknown as MockWorker;
        expect(mockWorker.receivedMessages).toHaveLength(1);

        const { msg, transfer } = mockWorker.receivedMessages[0];
        expect(msg.type).toBe(SELF_WORKER_MESSAGES.INIT);
        expect(msg.settings).toBe(dummySettings);

        // Resources must contain the URLs of the WASM binaries.
        expect(msg.resources).toMatchObject({
            wasmUrl: WASM_URL.href,
            wasmDspUrl: WASM_DSP_URL.href,
        });

        // The models object must keep ArrayBuffers and convert URLs to hrefs.
        expect(msg.resources.models).toMatchObject({
            noiseCancellation: {
                fast24: transfer[0],
                quality24: 'https://example.com/model.gdn',
            },
        });

        // The transfer list must include the ArrayBuffer from the models.
        expect(transfer).toHaveLength(1);
        expect(transfer[0]).not.toBe(rawModels.noiseCancellation!.fast24);
        expect(rawModels.noiseCancellation!.fast24.byteLength).toBe(4);
    });

    test('createPort returns a port that is destroyed when the last client closes', async () => {
        const worker = new GigaDenoiserWorker(dummyLogger, dummyTelemetry, dummySettings, undefined);

        // Spy on the worker's destroy method.
        const destroySpy = jest.spyOn(worker, 'destroy');

        const port1 = worker.createPort() as unknown as MessagePort;
        const port2 = worker.createPort() as unknown as MessagePort;

        const postDestroySpy = jest.spyOn(worker, 'postMessage');

        // Close the first port – worker should still be alive.
        port1.close();
        expect(destroySpy).not.toHaveBeenCalled();

        // Close the second port – worker should be destroyed.
        port2.close();
        expect(destroySpy).toHaveBeenCalledTimes(1);

        // The last close also sends a DESTROY message to the worker.
        expect(postDestroySpy).toHaveBeenCalledWith({ type: SELF_WORKER_MESSAGES.DESTROY });
    });
});
