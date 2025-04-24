import { CONTROLLER_MESSAGES, ControllerMessage, CustomPort } from './ipc';
import { TELEMETRY_FRAME_TIME_MAX_COUNT } from './constants';

export interface MetricNameToPayloadMap {
    totalTimeAndFrameCount: { count: number; time: number };
    frameProcessTimeRaw: number[];
    initTime: number;

    wasmDownloadTime: number;
    wasmDSPDownloadTime: number;
    modelsDataDownloadTime: number;
}

export type SendMetricFunction = <T extends keyof MetricNameToPayloadMap>(
    name: T,
    payload: MetricNameToPayloadMap[T],
) => void;

export type Telemetry = {
    enabled: boolean;
    sendMetric: SendMetricFunction;
};

// TODO: own telemetry (submit to eye sbdv)
export function createTelemetry(sendMetric?: SendMetricFunction): Telemetry {
    return {
        enabled: Boolean(sendMetric),
        sendMetric: sendMetric || (() => {}),
    };
}

export type WorkerTelemetry = {
    enabled: boolean;
    recordFrameProcessTime(time: number): void;
    flush(): void;
};

type WorkletTelemetry = {
    enabled: boolean;
    frameReceived(): void;

    start(time: number): void;
    stop(time: number): void;
};

export function getWorkerTelemetry(enabled: boolean, port: CustomPort<any, ControllerMessage>): WorkerTelemetry {
    let times = [] as number[];

    return {
        enabled,
        recordFrameProcessTime(time: number) {
            if (!this.enabled || enabled) return;

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

export function getWorkletTelemetry(enabled: boolean, port: CustomPort<any, ControllerMessage>): WorkletTelemetry {
    let started = false;
    let startTime = 0;
    let chunkCount = 0;

    return {
        enabled,
        frameReceived() {
            chunkCount++;
        },
        start(time) {
            this.stop(time);
            startTime = time;
            chunkCount = 0;
            started = true;
        },
        stop(time) {
            if (!this.enabled) return;

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
