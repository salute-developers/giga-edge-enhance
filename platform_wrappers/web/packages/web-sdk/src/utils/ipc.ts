import { MetricNameToPayloadMap } from './telemetry';

export const WORKER_MESSAGES = {
    // workletPort, wasm, data, sampleRate, options
    INIT: 'init',
    DESTROY: 'destroy',

    // from worklet
    RESET: 'reset',
    INPUT: 'input',
} as const;

export const WORKLET_MESSAGES = {
    // TODO unify
    // main worklet
    INIT: 'init',
    // normalizer worklet
    INIT_NORM: 'init_norm',
    ENABLE: 'enable',
    DISABLE: 'disable',
    DESTROY: 'destroy',

    // From worker
    OUTPUT: 'output',
    WORKER_READY: 'worker_ready',
} as const;

export const CONTROLLER_MESSAGES = {
    OVERFLOW: 'overflow',
    TELEMETRY: 'telemetry',
    LOG: 'log',
    SYSTEM_READY: 'sys_ready',
} as const;

export type ControllerToWorkletMessage =
    // TODO unify
    // Normalizer worklet
    | {
          type: typeof WORKLET_MESSAGES.INIT_NORM;
          wasm: ArrayBuffer;
      }
    // Spench worklet
    | {
          type: typeof WORKLET_MESSAGES.INIT;
          workerPort: MessagePort;
      }
    | {
          type: typeof WORKLET_MESSAGES.ENABLE | typeof WORKLET_MESSAGES.DISABLE | typeof WORKLET_MESSAGES.DESTROY;
      };

export type ControllerToWorkerMessage =
    | {
          type: typeof WORKER_MESSAGES.INIT;
          wasm: ArrayBuffer;
          data: ArrayBuffer;
          sampleRate: number;
          workletPort: MessagePort;
      }
    | { type: typeof WORKER_MESSAGES.DESTROY };

export type CommonToControllerMessage<T extends keyof MetricNameToPayloadMap = keyof MetricNameToPayloadMap> =
    | {
          type: typeof CONTROLLER_MESSAGES.LOG;
          level: 'debug' | 'error' | 'warn';
          data: unknown[];
      }
    | {
          type: typeof CONTROLLER_MESSAGES.TELEMETRY;
          metricName: T;
          value: MetricNameToPayloadMap[T];
      };

export type WorkerToControllerMessage = CommonToControllerMessage;

export type WorkerToWorkletMessage =
    | { type: typeof WORKLET_MESSAGES.WORKER_READY }
    | { type: typeof WORKLET_MESSAGES.OUTPUT; output: Float32Array };

export type WorkletToControllerMessage =
    | CommonToControllerMessage
    | { type: typeof CONTROLLER_MESSAGES.SYSTEM_READY }
    | {
          type: typeof CONTROLLER_MESSAGES.OVERFLOW;
          overflowCount: number;
      };

export type WorkletToWorkerMessage =
    | { type: typeof WORKER_MESSAGES.RESET }
    | { type: typeof WORKER_MESSAGES.INPUT; input: Float32Array };

export interface CommonControllerPort {
    postMessage(message: CommonToControllerMessage, transfer: Transferable[]): void;
    postMessage(message: CommonToControllerMessage, options?: StructuredSerializeOptions): void;
}
