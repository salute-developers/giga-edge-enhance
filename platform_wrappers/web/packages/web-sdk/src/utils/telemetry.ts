import { Logger } from './logger';
import { CommonControllerPort, CONTROLLER_MESSAGES } from './ipc';
import { TELEMETRY_FRAME_TIME_MAX_COUNT } from './constants';

export interface MetricNameToPayloadMap {
    totalTimeAndFrameCount: { count: number; time: number };
    frameProcessTimeRaw: number[];
    initTime: number;

    wasmDownloadTime: number;
    modelsDataDownloadTime: number;
}

export type SendMetricFunction = <T extends keyof MetricNameToPayloadMap>(
    name: T,
    payload: MetricNameToPayloadMap[T],
) => void;

export type AudioNodeTelemetry = {
    sendMetric: SendMetricFunction;
};

// TODO: own telemetry (submit to eye sbdv)
export function createAudioNodeTelemetry(logger: Logger, sendMetric?: SendMetricFunction): AudioNodeTelemetry {
    const fallbackSendMetric: SendMetricFunction = (name, payload) => logger.debug('metric', name, payload);

    return {
        sendMetric: sendMetric || fallbackSendMetric,
    };
}

export type LoaderTelemetry = AudioNodeTelemetry;

export const createLoaderTelemetry: (logger: Logger, sendMetric?: SendMetricFunction) => LoaderTelemetry =
    createAudioNodeTelemetry;

type WorkerTelemetry = {
    recordFrameProcessTime(time: number): void;
    flush(): void;
};

type WorkletTelemetry = {
    frameReceived(): void;

    start(time: number): void;
    stop(time: number): void;
};

export function getWorkerTelemetry(port: CommonControllerPort): WorkerTelemetry {
    let times = [] as number[];

    return {
        recordFrameProcessTime(time: number) {
            times.push(time);

            if (times.length > TELEMETRY_FRAME_TIME_MAX_COUNT) {
                this.flush();
            }
        },
        flush() {
            if (times.length <= 0) {
                return;
            }

            port.postMessage({
                type: CONTROLLER_MESSAGES.TELEMETRY,
                metricName: 'frameProcessTimeRaw',
                value: times,
            });
            times = [];
        },
    };
}

export function getWorkletTelemetry(port: CommonControllerPort): WorkletTelemetry {
    let started = false;
    let startTime = 0;
    let chunkCount = 0;

    return {
        frameReceived() {
            chunkCount++;
        },
        start(time) {
            startTime = time;
            chunkCount = 0;
            started = true;
        },
        stop(time) {
            if (chunkCount > 0 && started && time > startTime) {
                port.postMessage({
                    type: CONTROLLER_MESSAGES.TELEMETRY,
                    metricName: 'totalTimeAndFrameCount',
                    value: {
                        count: chunkCount,
                        time: time - startTime,
                    },
                });
                started = false;
            }
        },
    };
}
