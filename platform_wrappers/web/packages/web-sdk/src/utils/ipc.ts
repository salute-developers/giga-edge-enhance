import { ErrorObject } from 'serialize-error';

import { MetricNameToPayloadMap } from './telemetry';
import { Settings, DspFeatures, Features, ModelsTmpl } from './common';
import { LogLevel, Verbosity } from './logger';

export type Message<N, D = unknown> = D & {
    type: N;
};

export const WORKER_MESSAGES = {
    INIT: 'init',
    DESTROY: 'destroy',
    SET_FEATURES: 'set_features',
    RESET: 'reset',

    // from worklet
    INPUT: 'input',
} as const;

export const WORKLET_MESSAGES = {
    INIT: 'init',
    RESET: 'reset',
    SET_FEATURES: 'set_features',
    WORKER_CREATED: 'worker_created',
    DESTROY: 'destroy',

    // From worker
    OUTPUT: 'output',
} as const;

export const CONTROLLER_MESSAGES = {
    LOW_PERFORMANCE: 'low_performance',
    SET_FEATURES_RESULT: 'set_features_result',
    TELEMETRY: 'telemetry',
    LOG: 'log',
    ERROR: 'error',
} as const;

export const SELF_WORKER_MESSAGES = {
    INIT: 'init',
    NEW_CLIENT: 'new_client',
    DESTROY: 'destroy',
} as const;

export type ResourceUrls = {
    wasmUrl: string;
    wasmDspUrl: string;
    models: ModelsTmpl<string | ArrayBuffer> | undefined;
};

export type SelfWorkerMessage =
    | Message<
          typeof SELF_WORKER_MESSAGES.INIT,
          {
              verbosity: Verbosity;
              isTelemetryEnabled: boolean;
              resources: ResourceUrls;
              settings: Settings;
          }
      >
    | Message<typeof SELF_WORKER_MESSAGES.NEW_CLIENT, { port: CustomPort<WorkerMessage, ControllerMessage> }>
    | Message<typeof SELF_WORKER_MESSAGES.DESTROY>;

export type WorkerControllerMessage = LogMessage;

export type WorkletMessage =
    | Message<typeof WORKLET_MESSAGES.INIT, { verbosity: Verbosity; isTelemetryEnabled: boolean; settings: Settings }>
    | Message<typeof WORKLET_MESSAGES.WORKER_CREATED, { workerPort: MessagePort }>
    | Message<
          typeof WORKLET_MESSAGES.SET_FEATURES,
          { workerFeaturesEnabled: boolean; dspWasm?: ArrayBuffer; features?: DspFeatures }
      >
    | Message<typeof WORKLET_MESSAGES.OUTPUT, { output: Float32Array }>
    | Message<typeof WORKLET_MESSAGES.RESET, { telemetry?: boolean }>
    | Message<typeof WORKLET_MESSAGES.DESTROY>;

export type WorkerMessage =
    | Message<
          typeof WORKER_MESSAGES.INIT,
          {
              sampleRate: number;
              workletPort: MessagePort;
          }
      >
    | Message<typeof WORKER_MESSAGES.SET_FEATURES, { features: Features<ModelsTmpl> }>
    | Message<typeof WORKER_MESSAGES.INPUT, { input: Float32Array; loopback: Float32Array }>
    | Message<typeof WORKER_MESSAGES.DESTROY>
    | Message<typeof WORKER_MESSAGES.RESET>;

export type LogMessage = Message<typeof CONTROLLER_MESSAGES.LOG, { level: LogLevel; data: unknown[] }>;

export type ControllerMessage<T extends keyof MetricNameToPayloadMap = keyof MetricNameToPayloadMap> =
    | LogMessage
    | Message<
          typeof CONTROLLER_MESSAGES.SET_FEATURES_RESULT,
          { errors: ErrorObject[]; features: Partial<Features<ModelsTmpl>> }
      >
    | Message<typeof CONTROLLER_MESSAGES.ERROR, { error: ErrorObject }>
    | Message<typeof CONTROLLER_MESSAGES.TELEMETRY, { metricName: T; value: MetricNameToPayloadMap[T] }>
    | Message<typeof CONTROLLER_MESSAGES.LOW_PERFORMANCE>;

export interface CustomPort<I extends Message<string>, O extends Message<string>> extends MessagePort {
    onmessage: ((this: CustomPort<I, O>, event: MessageEvent<I>) => void) | null;
    addEventListener<M extends { message: MessageEvent<I>; messageerror: MessageEvent }, K extends keyof M>(
        type: K,
        listener: (this: CustomPort<I, O>, ev: M[K]) => any,
        options?: boolean | AddEventListenerOptions,
    ): void;
    removeEventListener<M extends { message: MessageEvent<I>; messageerror: MessageEvent }, K extends keyof M>(
        type: K,
        listener: (this: CustomPort<I, O>, ev: M[K]) => any,
        options?: boolean | EventListenerOptions,
    ): void;

    postMessage(message: O, transfer: Transferable[]): void;
    postMessage(message: O, options?: StructuredSerializeOptions): void;
}
