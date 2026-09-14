import {
    CONTROLLER_MESSAGES,
    ControllerMessage,
    CustomPort,
    SELF_WORKER_MESSAGES,
    SelfWorkerMessage,
    WORKER_MESSAGES,
    WorkerControllerMessage,
    WorkerMessage,
} from './utils/ipc';
import { Logger } from './utils/logger';
import { assert } from './utils/error';
import { Telemetry } from './utils/telemetry';
import { WASM_DSP_URL, WASM_URL } from './utils/resources';
import { copyArrayBuffer, ModelsTmpl, Settings } from './utils/common';
import { WORKER_URL } from './utils/jsResources';

let currentWorkerNumber = 0;

export function getTransferableModelRepresentation<M extends ModelsTmpl>(
    rawModels?: M,
): [ModelsTmpl<string | ArrayBuffer> | undefined, Transferable[]] {
    if (!rawModels) {
        return [undefined, []];
    }
    const transferable = [] as Transferable[];

    const models = Object.fromEntries(
        Object.entries(rawModels).map(([kind, modelTypeMap]) => [
            kind,
            Object.fromEntries(
                Object.entries(modelTypeMap).map(([modelType, resourceValue]) => {
                    if (resourceValue instanceof ArrayBuffer) {
                        const ownedBuffer = copyArrayBuffer(resourceValue);
                        transferable.push(ownedBuffer);

                        return [modelType, ownedBuffer];
                    }

                    return [modelType, resourceValue instanceof URL ? resourceValue.href : resourceValue];
                }),
            ),
        ]),
    ) as ModelsTmpl<string | ArrayBuffer>;

    return [models, transferable];
}

interface ISpenchWorker extends Worker {
    onmessage: ((this: Worker, event: MessageEvent<WorkerControllerMessage>) => void) | null;
    addEventListener<
        M extends { message: MessageEvent<WorkerControllerMessage>; messageerror: MessageEvent },
        K extends keyof M,
    >(
        type: K,
        listener: (this: Worker, ev: M[K]) => any,
        options?: boolean | AddEventListenerOptions,
    ): void;
    removeEventListener<
        M extends { message: MessageEvent<WorkerControllerMessage>; messageerror: MessageEvent },
        K extends keyof M,
    >(
        type: K,
        listener: (this: Worker, ev: M[K]) => any,
        options?: boolean | EventListenerOptions,
    ): void;
}

class SpenchWorker<M extends ModelsTmpl = ModelsTmpl> extends Worker implements ISpenchWorker {
    weak = true;

    protected clients: number = 0;

    protected workerNumber = currentWorkerNumber++;

    protected portNumber = 0;

    constructor(
        protected readonly logger: Logger,
        protected readonly telemetry: Telemetry,
        settings: Settings,
        models?: M,
    ) {
        super(WORKER_URL);
        const [transferableModels, transferable] = getTransferableModelRepresentation(models);
        const resources = {
            wasmUrl: WASM_URL.href,
            wasmDspUrl: WASM_DSP_URL.href,
            models: transferableModels,
        };

        this.postMessage(
            {
                type: SELF_WORKER_MESSAGES.INIT,
                resources,
                verbosity: this.logger.verbosity,
                isTelemetryEnabled: telemetry.enabled,
                settings,
            },
            transferable,
        );

        this.addEventListener('message', this.#handleMessage);
        this.addEventListener('error', this.#handleError);

        this.logger.debug(`worker #${this.workerNumber} created`);
    }

    postMessage(message: SelfWorkerMessage, transfer: Transferable[]): void;

    postMessage(message: SelfWorkerMessage, options?: StructuredSerializeOptions): void;

    postMessage(message: SelfWorkerMessage, options?: any): void {
        return super.postMessage(message, options);
    }

    createPort(): CustomPort<ControllerMessage, WorkerMessage> {
        const channel = new MessageChannel();
        const currentNumber = this.portNumber++;

        const { port1 } = channel;
        this.postMessage({ type: SELF_WORKER_MESSAGES.NEW_CLIENT, port: port1 }, [port1]);

        this.clients++;
        // eslint-disable-next-line prefer-destructuring
        const port2: CustomPort<any, WorkerMessage> = channel.port2;
        const originalClose = port2.close;
        port2.close = () => {
            this.logger.debug(`[worker #${this.workerNumber}] port #${currentNumber} closed`);
            port2.close = originalClose;
            this.clients--;
            port2.postMessage({ type: WORKER_MESSAGES.DESTROY });
            originalClose.call(port2);

            if (this.weak && this.clients <= 0) {
                this.destroy();
            }
        };

        this.logger.debug(`[worker #${this.workerNumber}] port #${currentNumber} created`);

        return port2;
    }

    destroy(): void {
        this.postMessage({ type: SELF_WORKER_MESSAGES.DESTROY });
        this.removeEventListener('message', this.#handleMessage);
        this.removeEventListener('error', this.#handleError);

        this.logger.debug(`worker #${this.workerNumber} destroyed`);
    }

    #handleMessage = (event: MessageEvent<ControllerMessage>) => {
        switch (event.data.type) {
            case CONTROLLER_MESSAGES.LOG:
                this.logger[event.data.level](...event.data.data);

                return;
            case CONTROLLER_MESSAGES.TELEMETRY:
                if (event.data.metricName === 'currentMeanProcessTime') {
                    this.telemetry.currentMeanProcessTime = event.data.value as number;

                    return true;
                }

                this.telemetry.sendMetric(event.data.metricName, event.data.value);

                return true;
            default:
        }

        assert(false, `Unknown message type from Worker: ${event.data.type}`);
    };

    #handleError = (event: ErrorEvent) => {
        this.logger.error(
            `${event.error || event.message || 'Unknown error (check resource accessibility)'} at SpenchWorker`,
        );
        this.destroy();
    };
}

export default SpenchWorker;
