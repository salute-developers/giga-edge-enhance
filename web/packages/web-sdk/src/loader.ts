import GigaDenoiserError from './utils/error';
import {
    downloadFileAsBuffer,
    retry,
    Features,
    copyArrayBuffer,
    Settings,
    ModelsTmpl,
    getDspFeatures,
    getMainFeatures,
    MainFeatures,
    ModelKey,
} from './utils/common';
import { Logger } from './utils/logger';
import { Telemetry } from './utils/telemetry';
import GigaDenoiserWorker from './worker';
import { WASM_DSP_URL, WASM_URL } from './utils/resources';
import { WORKLET_DSP_URL, WORKLET_URL } from './utils/jsResources';

export class LoadError extends GigaDenoiserError {
    override get name() {
        return 'LoadError';
    }
}

async function preloadResource(url: string | URL): Promise<void> {
    const response = await fetch(url, { priority: 'low' });

    if (!response.ok) {
        throw new Error();
    }
}

class Loader<M extends ModelsTmpl> {
    protected static wasmPreloadPromise: Promise<void> | undefined;

    protected static dataPreloadPromise: Promise<void> | undefined;

    protected static dspWasmPreloadPromise: Promise<void> | undefined;

    protected static dspWasmLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    protected static modelsLoadPromiseMap: Map<string, Promise<void>> = new Map();

    preloadedWorker: GigaDenoiserWorker | LoadError | undefined;

    protected preloadedWorkerSettings: Settings | undefined;

    workletLoadPromise: Promise<void | LoadError> | undefined;

    dspWorkletLoadPromise: Promise<void | LoadError> | undefined;

    constructor(
        protected ctx: AudioContext,
        protected logger: Logger,
        protected telemetry: Telemetry,
        protected settings: Settings,
        public readonly models: M | undefined,
    ) {}

    protected static preloadWasm(): Promise<void> {
        const load = async () => {
            try {
                await preloadResource(WASM_URL);
            } catch {
                Loader.wasmPreloadPromise = undefined;
            }
        };

        return Loader.wasmPreloadPromise || (Loader.wasmPreloadPromise = load());
    }

    protected static preloadDspWasm(): Promise<void> {
        const load = async () => {
            try {
                await preloadResource(WASM_DSP_URL);
            } catch {
                Loader.dspWasmPreloadPromise = undefined;
            }
        };

        return Loader.dspWasmPreloadPromise || (Loader.dspWasmPreloadPromise = load());
    }

    // eslint-disable-next-line no-shadow
    protected static async preloadModels<M extends ModelsTmpl>(
        features: Partial<Features<M>>,
        models?: M,
    ): Promise<void> {
        if (!models) {
            return;
        }

        const filteredFeatures = Object.entries(getMainFeatures(features)).filter(
            ([, value]) => typeof value === 'string',
        ) as [keyof MainFeatures<M>, ModelKey][];

        const preloadRequests = filteredFeatures
            .map(([feature, modelType]) => models[feature]?.[modelType])
            .filter((value) => typeof value === 'string' || value instanceof URL)
            .map(async (url) => {
                const urlString = typeof url === 'string' ? url : url.href;

                if (Loader.modelsLoadPromiseMap.get(urlString)) {
                    return;
                }
                const req = preloadResource(url);
                Loader.modelsLoadPromiseMap.set(urlString, req);

                try {
                    await req;
                } catch {
                    Loader.modelsLoadPromiseMap.delete(urlString);
                }

                return req;
            });

        await Promise.all(preloadRequests);
    }

    preload(features: Partial<Features<M>>, settings: Settings = this.settings): void {
        this.loadGigaDenoiserWorklet(settings);

        if (Object.values(getDspFeatures(features)).some(Boolean)) {
            if (settings.dspOnAudioThread) {
                this.loadDSPWasm(true);
            } else {
                Loader.preloadDspWasm();
            }

            if (!settings.dspOnAudioThread) {
                this.loadWorker(settings, true);
            }
        }

        if (Object.values(getMainFeatures(features)).some(Boolean)) {
            Loader.preloadWasm();
            Loader.preloadModels(features, this.models);
            this.loadWorker(settings, true);
        }
    }

    async loadDSPWasm(preload = false): Promise<ArrayBuffer | LoadError> {
        const load = async () => {
            try {
                const [buffer, time] = await downloadFileAsBuffer(WASM_DSP_URL, preload);
                this.telemetry.sendMetric('wasmDSPDownloadTime', time);

                return buffer;
            } catch (e) {
                Loader.dspWasmLoadPromise = undefined;

                return new LoadError(e);
            }
        };

        const res = await retry(
            () => Loader.dspWasmLoadPromise || (Loader.dspWasmLoadPromise = load()),
            preload ? 0 : this.settings.fetchRetries,
        );

        if (res instanceof ArrayBuffer) {
            // Make sure AB not transfered
            return copyArrayBuffer(res);
        }

        return res as LoadError;
    }

    async loadWorker(settings: Settings = this.settings, preload = false): Promise<GigaDenoiserWorker | LoadError> {
        if (
            this.preloadedWorker instanceof Worker &&
            JSON.stringify(this.preloadedWorkerSettings) === JSON.stringify(settings)
        ) {
            return this.preloadedWorker;
        }

        if (this.preloadedWorker instanceof Worker) {
            this.preloadedWorker.terminate();
        }
        this.preloadedWorker = undefined;
        this.preloadedWorkerSettings = undefined;

        const load = () => {
            try {
                return new GigaDenoiserWorker(this.logger, this.telemetry, settings, this.models);
            } catch (e) {
                return new LoadError(e);
            }
        };

        return retry(async () => {
            this.preloadedWorker = load();
            this.preloadedWorkerSettings = settings;
            return this.preloadedWorker;
        }, preload ? 0 : settings.fetchRetries) as Promise<
            GigaDenoiserWorker | LoadError
        >;
    }

    loadGigaDenoiserWorklet(settings: Settings): Promise<void | LoadError> {
        if (settings.dspOnAudioThread) {
            return this.#loadGigaDenoiserDspWorklet();
        }

        return this.#loadGigaDenoiserWorklet();
    }

    #loadGigaDenoiserWorklet(preload = false): Promise<void | LoadError> {
        const load = async (): Promise<void | LoadError> => {
            try {
                this.logger.debug('loading GigaDenoiser worklet');
                await this.ctx.audioWorklet.addModule(WORKLET_URL);
            } catch (e) {
                this.workletLoadPromise = undefined;
                this.logger.debug(e, (e as Error).message, (e as Error).stack, (e as Error).name, WORKLET_URL);

                return new LoadError(e);
            }

            this.logger.debug('loading GigaDenoiser worklet finished');
        };

        return retry(
            () => this.workletLoadPromise || (this.workletLoadPromise = load()),
            preload ? 0 : this.settings.fetchRetries,
        ) as Promise<void | LoadError>;
    }

    #loadGigaDenoiserDspWorklet(): Promise<void | LoadError> {
        const load = async (): Promise<void | LoadError> => {
            try {
                this.logger.debug('loading GigaDenoiser DSP worklet');
                await this.ctx.audioWorklet.addModule(WORKLET_DSP_URL);
            } catch (e) {
                this.dspWorkletLoadPromise = undefined;

                return new LoadError(e);
            }

            this.logger.debug('loading GigaDenoiser DSP worklet finished');
        };

        return retry(
            () => this.dspWorkletLoadPromise || (this.dspWorkletLoadPromise = load()),
            this.settings.fetchRetries,
        ) as Promise<void | LoadError>;
    }
}

export default Loader;
