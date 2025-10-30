import { downloadFileAsBuffer, retry, Settings } from '../../utils/common';
import { WorkerTelemetry } from '../../utils/telemetry';
import { ResourceUrls } from '../../utils/ipc';
import SpenchError from '../../utils/error';

export class LoadError extends SpenchError {}

class Loader {
    wasmLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    dataLoadPromise: Promise<ArrayBuffer | LoadError> | undefined;

    constructor(
        protected resourceUrls: ResourceUrls,
        protected telemetry: WorkerTelemetry,
        protected settings: Settings,
    ) {}

    loadWasm(): Promise<ArrayBuffer | LoadError> {
        return (this.wasmLoadPromise =
            this.wasmLoadPromise || this.loadResource(this.resourceUrls.wasmUrl, 'wasmDownloadTime'));
    }

    loadDSPWasm(): Promise<ArrayBuffer | LoadError> {
        return this.loadResource(this.resourceUrls.wasmDspUrl, 'wasmDSPDownloadTime');
    }

    loadModelsData(): Promise<ArrayBuffer | LoadError> {
        return (this.dataLoadPromise =
            this.dataLoadPromise || this.loadResource(this.resourceUrls.dataUrl, 'modelsDataDownloadTime'));
    }

    protected loadResource(
        url: string,
        metricName: Parameters<WorkerTelemetry['sendMetric']>[0],
    ): Promise<ArrayBuffer | LoadError> {
        const load = async () => {
            try {
                const [buffer, time] = await downloadFileAsBuffer(url);
                this.telemetry.sendMetric(metricName, time);

                return buffer;
            } catch (e) {
                return new LoadError(e);
            }
        };

        return retry(load, this.settings.fetchRetries) as Promise<ArrayBuffer | LoadError>;
    }
}

export default Loader;
