import SpenchError from './utils/error';
import { downloadFileAsBuffer, retry, Features, copyArrayBuffer } from './utils/common';
import { Logger } from './utils/logger';
import { Telemetry } from './utils/telemetry';
import SpenchWorker from './worker';

export class LoadError extends SpenchError {}

class Loader {
    #wasmLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    #dspWasmLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    #dataLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    #preloadedWorker: SpenchWorker | LoadError | undefined;

    #workletLoadPromise: Promise<void | LoadError> | undefined;

    #fullWorkerLoadPromise: Promise<SpenchWorker | LoadError> | undefined;

    constructor(
        private ctx: AudioContext,
        private logger: Logger,
        private telemetry: Telemetry,
    ) {}

    loadWorker() {
        const load = async () => {
            this.logger.debug('loading Spench worker');
            const wasmPromise = retry(() => this.#loadWasm());
            const dataPromise = retry(() => this.#loadModelsData());
            const workerPromise = retry(() => this.#createWorker());

            const res = await Promise.all([wasmPromise, dataPromise, workerPromise]);

            for (const r of res) {
                if (r instanceof Error) {
                    this.#fullWorkerLoadPromise = undefined;

                    return new LoadError(r);
                }
            }

            const [wasm, data, worker] = res as [ArrayBuffer, ArrayBuffer, SpenchWorker];

            worker.init(wasm, data);

            this.logger.debug('loading Spench worker finished');
            // Can't reuse ArrayBuffer as it's transferred to the different context
            // Also binaries are quite big, so we rely on the browser cache
            this.#wasmLoadPromise = undefined;
            this.#dataLoadPromise = undefined;
            this.#preloadedWorker = undefined;

            return worker;
        };

        return this.#fullWorkerLoadPromise || (this.#fullWorkerLoadPromise = load());
    }

    preload(features: Partial<Features>): void {
        this.loadSpenchWorklet();

        if (features.normalization || features.echoCancellation) {
            this.loadDSPWasm();
        }

        if (features.noiseCancellation || features.dereverberation) {
            this.loadWorker();
        }
    }

    loadSpenchWorklet() {
        const load = async (): Promise<void | LoadError> => {
            try {
                this.logger.debug('loading Spench worklet');
                await this.ctx.audioWorklet.addModule(
                    new URL('../dist/workers/spench-worklet.min.js', import.meta.url),
                );
            } catch {
                this.#workletLoadPromise = undefined;

                return new LoadError();
            }

            this.logger.debug('loading Spench worklet finished');
        };

        return retry(
            () => this.#workletLoadPromise || (this.#workletLoadPromise = load()),
        ) as Promise<void | LoadError>;
    }

    async loadDSPWasm(): Promise<ArrayBuffer | LoadError> {
        const load = async () => {
            try {
                const [buffer, time] = await downloadFileAsBuffer(new URL('../bin/spench.dsp.wasm', import.meta.url));
                this.telemetry.sendMetric('wasmDSPDownloadTime', time);

                return buffer;
            } catch (e) {
                this.#dspWasmLoadPromise = undefined;

                return new LoadError(e);
            }
        };

        const res = await retry(() => this.#dspWasmLoadPromise || (this.#dspWasmLoadPromise = load()));

        if (res instanceof ArrayBuffer) {
            // Make sure AB not transfered
            return copyArrayBuffer(res);
        }

        return res as LoadError;
    }

    #loadWasm(): Promise<ArrayBuffer | LoadError> {
        const load = async () => {
            try {
                const [buffer, time] = await downloadFileAsBuffer(new URL('../bin/spench.wasm', import.meta.url));
                this.telemetry.sendMetric('wasmDownloadTime', time);

                return buffer;
            } catch (e) {
                this.#wasmLoadPromise = undefined;

                return new LoadError(e);
            }
        };

        return this.#wasmLoadPromise || (this.#wasmLoadPromise = load());
    }

    #loadModelsData(): Promise<ArrayBuffer | LoadError> {
        const load = async () => {
            try {
                const [buffer, time] = await downloadFileAsBuffer(new URL('../bin/spench.data', import.meta.url));
                this.telemetry.sendMetric('modelsDataDownloadTime', time);

                return buffer;
            } catch (e) {
                this.#dataLoadPromise = undefined;

                return new LoadError(e);
            }
        };

        return this.#dataLoadPromise || (this.#dataLoadPromise = load());
    }

    async #createWorker(): Promise<SpenchWorker | LoadError> {
        if (this.#preloadedWorker instanceof Worker) {
            return this.#preloadedWorker;
        }

        const load = () => {
            try {
                return new SpenchWorker(this.logger, this.telemetry);
            } catch (e) {
                return new LoadError(e);
            }
        };

        return (this.#preloadedWorker = load());
    }
}

export default Loader;
