import SpenchError from './utils/error';
import { downloadFileAsBuffer, retry, Features, copyArrayBuffer, Settings } from './utils/common';
import { Logger } from './utils/logger';
import { Telemetry } from './utils/telemetry';
import SpenchWorker from './worker';
import { dataUrl, wasmDspUrl, wasmUrl, workletDspUrl, workletUrl } from './utils/resources';

export class LoadError extends SpenchError {}

class Loader {
    static #wasmPreloadPromise: Promise<void> | undefined;

    static #dataPreloadPromise: Promise<void> | undefined;

    static #dspWasmPreloadPromise: Promise<void> | undefined;

    static #dspWasmLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    preloadedWorker: SpenchWorker | LoadError | undefined;

    workletLoadPromise: Promise<void | LoadError> | undefined;

    dspWorkletLoadPromise: Promise<void | LoadError> | undefined;

    constructor(
        private ctx: AudioContext,
        private logger: Logger,
        private telemetry: Telemetry,
        private settings: Settings,
    ) {}

    static #preloadWasm(): Promise<void> {
        const load = async () => {
            try {
                const response = await fetch(wasmUrl, { priority: 'low' });

                if (!response.ok) {
                    throw new Error();
                }
            } catch {
                Loader.#wasmPreloadPromise = undefined;
            }
        };

        return Loader.#wasmPreloadPromise || (Loader.#wasmPreloadPromise = load());
    }

    static #preloadModelsData(): Promise<void> {
        const load = async () => {
            try {
                const response = await fetch(dataUrl, { priority: 'low' });

                if (!response.ok) {
                    throw new Error();
                }
            } catch {
                Loader.#dataPreloadPromise = undefined;
            }
        };

        return Loader.#dataPreloadPromise || (Loader.#dataPreloadPromise = load());
    }

    static #preloadDspWasm(): Promise<void> {
        const load = async () => {
            try {
                const response = await fetch(wasmDspUrl, { priority: 'low' });

                if (!response.ok) {
                    throw new Error();
                }
            } catch {
                Loader.#dspWasmPreloadPromise = undefined;
            }
        };

        return Loader.#dspWasmPreloadPromise || (Loader.#dspWasmPreloadPromise = load());
    }

    preload(features: Partial<Features>, settings: Settings): void {
        this.loadSpenchWorklet(settings);

        if (features.normalization || features.echoCancellation) {
            if (settings.dspOnAudioThread) {
                this.loadDSPWasm(true);
            } else {
                Loader.#preloadDspWasm();
            }

            if (!settings.dspOnAudioThread) {
                this.loadWorker(true);
            }
        }

        if (features.noiseCancellation || features.dereverberation) {
            Loader.#preloadWasm();
            Loader.#preloadModelsData();
            this.loadWorker(true);
        }
    }

    async loadDSPWasm(preload = false): Promise<ArrayBuffer | LoadError> {
        const load = async () => {
            try {
                const [buffer, time] = await downloadFileAsBuffer(wasmDspUrl, preload);
                this.telemetry.sendMetric('wasmDSPDownloadTime', time);

                return buffer;
            } catch (e) {
                Loader.#dspWasmLoadPromise = undefined;

                return new LoadError(e);
            }
        };

        const res = await retry(
            () => Loader.#dspWasmLoadPromise || (Loader.#dspWasmLoadPromise = load()),
            preload ? 0 : this.settings.fetchRetries,
        );

        if (res instanceof ArrayBuffer) {
            // Make sure AB not transfered
            return copyArrayBuffer(res);
        }

        return res as LoadError;
    }

    async loadWorker(preload = false): Promise<SpenchWorker | LoadError> {
        if (this.preloadedWorker instanceof Worker) {
            return this.preloadedWorker;
        }

        const load = () => {
            try {
                return new SpenchWorker(this.logger, this.telemetry, this.settings);
            } catch (e) {
                return new LoadError(e);
            }
        };

        return retry(async () => (this.preloadedWorker = load()), preload ? 0 : this.settings.fetchRetries) as Promise<
            SpenchWorker | LoadError
        >;
    }

    loadSpenchWorklet(settings: Settings): Promise<void | LoadError> {
        if (settings.dspOnAudioThread) {
            return this.#loadSpenchDspWorklet();
        }

        return this.#loadSpenchWorklet();
    }

    #loadSpenchWorklet(preload = false): Promise<void | LoadError> {
        const load = async (): Promise<void | LoadError> => {
            try {
                this.logger.debug('loading Spench worklet');
                await this.ctx.audioWorklet.addModule(workletUrl);
            } catch (e) {
                this.workletLoadPromise = undefined;

                return new LoadError(e);
            }

            this.logger.debug('loading Spench worklet finished');
        };

        return retry(
            () => this.workletLoadPromise || (this.workletLoadPromise = load()),
            preload ? 0 : this.settings.fetchRetries,
        ) as Promise<void | LoadError>;
    }

    #loadSpenchDspWorklet(): Promise<void | LoadError> {
        const load = async (): Promise<void | LoadError> => {
            try {
                this.logger.debug('loading Spench DSP worklet');
                await this.ctx.audioWorklet.addModule(workletDspUrl);
            } catch (e) {
                this.dspWorkletLoadPromise = undefined;

                return new LoadError(e);
            }

            this.logger.debug('loading Spench DSP worklet finished');
        };

        return retry(
            () => this.dspWorkletLoadPromise || (this.dspWorkletLoadPromise = load()),
            this.settings.fetchRetries,
        ) as Promise<void | LoadError>;
    }
}

export default Loader;
