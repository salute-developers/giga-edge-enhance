import { downloadFileAsBuffer, ModelKey, retry, Settings } from '../../utils/common';
import { WorkerTelemetry } from '../../utils/telemetry';
import { ResourceUrls } from '../../utils/ipc';
import GigaDenoiserError from '../../utils/error';

export class LoadError extends GigaDenoiserError {
    override get name() {
        return 'LoadError';
    }
}

export class ModelNotFoundError extends GigaDenoiserError {
    override get name() {
        return 'ModelNotFoundError';
    }
}

class Loader {
    constructor(
        protected resourceUrls: ResourceUrls,
        protected telemetry: WorkerTelemetry,
        protected settings: Settings,
    ) {}

    loadWasm(): Promise<ArrayBuffer | LoadError> {
        return this.loadResource(this.resourceUrls.wasmUrl, 'wasmDownloadTime');
    }

    loadDSPWasm(): Promise<ArrayBuffer | LoadError> {
        return this.loadResource(this.resourceUrls.wasmDspUrl, 'wasmDSPDownloadTime');
    }

    async loadNoiseCancellationModel(type: ModelKey): Promise<ArrayBuffer | ModelNotFoundError | LoadError> {
        const resource = this.resourceUrls.models?.noiseCancellation?.[type];

        if (!resource) {
            return new ModelNotFoundError(`Noise cancellation model not found for type ${type}`);
        }

        if (resource instanceof ArrayBuffer) {
            return resource;
        }

        return this.loadResource(resource);
    }

    loadResource(
        url: string,
        metricName?: Parameters<WorkerTelemetry['sendMetric']>[0],
    ): Promise<ArrayBuffer | LoadError> {
        const load = async () => {
            try {
                const [buffer, time] = await downloadFileAsBuffer(url);

                if (metricName) {
                    this.telemetry.sendMetric(metricName, time);
                }

                return buffer;
            } catch (e) {
                return new LoadError(e);
            }
        };

        return retry(load, this.settings.fetchRetries) as Promise<ArrayBuffer | LoadError>;
    }
}

export default Loader;
