import { CONTROLLER_MESSAGES, ControllerMessage, CustomPort } from './ipc';
import { MEAN_PROCESS_TIME_COUNT, TELEMETRY_FRAME_TIME_MAX_COUNT } from './constants';

export interface MetricNameToPayloadMap {
    // todo use repeated usage metric
    totalTimeAndFrameCount: { count: number; time: number };
    // todo use quartiles
    frameProcessTimeRaw: number[];
    // todo separate full/DSP
    initTime: number;

    wasmDownloadTime: number;
    wasmDSPDownloadTime: number;
    modelsDataDownloadTime: number;
    currentMeanProcessTime: number;
}

export type SendMetricFunction = <T extends keyof MetricNameToPayloadMap>(
    name: T,
    payload: MetricNameToPayloadMap[T],
) => void;

export type Telemetry = {
    enabled: boolean;
    currentMeanProcessTime: number | undefined;
    sendMetric: SendMetricFunction;
};

// TODO: own telemetry (submit to eye sbdv)
export function createTelemetry(sendMetric?: SendMetricFunction): Telemetry {
    return {
        enabled: Boolean(sendMetric),
        currentMeanProcessTime: undefined,
        sendMetric: sendMetric || (() => {}),
    };
}

export type WorkerTelemetry = {
    enabled: boolean;
    sendMetric: SendMetricFunction;
    recordFrameProcessTime(time: number): void;
    recordMeanFrameProcessTime(time: number): void;
    flush(): void;
};

type WorkletTelemetry = {
    enabled: boolean;
    started: boolean;
    frameReceived(): void;

    start(time: number): void;
    stop(time: number): void;
};

export function getWorkerTelemetry(
    enabled: boolean,
    port: Pick<CustomPort<any, ControllerMessage>, 'postMessage'>,
): WorkerTelemetry {
    let times = [] as number[];
    let currentMeanProcessTime = 0;
    let currentMeanProcessTimeCount = 0;

    return {
        enabled,
        recordFrameProcessTime(time: number) {
            if (!this.enabled || !enabled) return;

            times.push(time);

            if (times.length > TELEMETRY_FRAME_TIME_MAX_COUNT) {
                this.flush();
            }
        },
        recordMeanFrameProcessTime(time: number) {
            currentMeanProcessTimeCount++;
            currentMeanProcessTime += (time - currentMeanProcessTime) / currentMeanProcessTimeCount;

            if (currentMeanProcessTimeCount >= MEAN_PROCESS_TIME_COUNT) {
                this.sendMetric('currentMeanProcessTime', currentMeanProcessTime);

                currentMeanProcessTime = 0;
                currentMeanProcessTimeCount = 0;
            }
        },
        flush() {
            if (times.length <= 0) {
                return;
            }

            this.sendMetric('frameProcessTimeRaw', times);
            times = [];
        },
        sendMetric(metricName, value) {
            port.postMessage({
                type: CONTROLLER_MESSAGES.TELEMETRY,
                metricName,
                value,
            });
        },
    };
}

export function getWorkletTelemetry(enabled: boolean, port: CustomPort<any, ControllerMessage>): WorkletTelemetry {
    let startTime = 0;
    let chunkCount = 0;

    return {
        enabled,
        started: false,
        frameReceived() {
            chunkCount++;
        },
        start(time) {
            this.stop(time);
            startTime = time;
            chunkCount = 0;
            this.started = true;
        },
        stop(time) {
            if (!this.enabled) return;

            if (chunkCount > 0 && this.started && time > startTime) {
                port.postMessage({
                    type: CONTROLLER_MESSAGES.TELEMETRY,
                    metricName: 'totalTimeAndFrameCount',
                    value: {
                        count: chunkCount,
                        time: time - startTime,
                    },
                });
                this.started = false;
            }
        },
    };
}
