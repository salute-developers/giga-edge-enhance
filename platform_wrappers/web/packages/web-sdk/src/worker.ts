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

let currentWorkerNumber = 0;

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

class SpenchWorker extends Worker implements ISpenchWorker {
    weak = true;

    private clients: number = 0;

    private workerNumber = currentWorkerNumber++;

    private portNumber = 0;

    constructor(private readonly logger: Logger) {
        // Don't allow the bundler to process prepared worker
        const url = new URL('../dist/workers/spench-worker.min.js', import.meta.url);
        super(url);
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

    init(wasm: ArrayBuffer, data: ArrayBuffer): void {
        this.postMessage(
            {
                type: SELF_WORKER_MESSAGES.INIT,
                wasm,
                data,
            },
            [wasm, data],
        );

        this.addEventListener('message', this.#handleMessage);
        this.addEventListener('error', this.#handleError);
    }

    #handleMessage = (event: MessageEvent<ControllerMessage>) => {
        switch (event.data.type) {
            case CONTROLLER_MESSAGES.LOG:
                this.logger[event.data.level](...event.data.data);

                return;
            default:
        }

        assert(false, 'Unknown message type from Worker');
    };

    #handleError = (event: ErrorEvent) => {
        this.logger.error(`${event.error || event.message} at SpenchWorker`);
        this.destroy();
    };
}

export default SpenchWorker;
