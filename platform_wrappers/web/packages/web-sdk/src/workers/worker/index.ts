import Timer from '../../utils/timers';

import { createDenoiseService, DenoiseService, initSpenchWasmModule } from './denoiseService';

const timer = new Timer();
let workletPort: MessagePort | undefined;
let denoiseService: DenoiseService | undefined;
let chunkIndex = 0;
let nextToProcessId: NodeJS.Timeout | undefined;

function sendState(state: string, time: number) {
    postMessage({ type: 'state', state, time });
}

type WorkletMessage = { type: 'input'; input: Float32Array };

function onWorkletMessage(event: { data: WorkletMessage }) {
    switch (event.data.type) {
        case 'input':
            if (denoiseService) {
                denoiseService.pushInput(event.data.input);

                if (denoiseService.isOverloaded()) {
                    postMessage({ type: 'overloaded' });
                }
                // console.debug('spench input size', denoiseService.getInputSize());

                if (denoiseService.hasEnoughInput() && !nextToProcessId) {
                    /** Schedule all available input data and only then try to process it. */
                    nextToProcessId = setTimeout(async function processNext() {
                        timer.lap();
                        const outputChunk = denoiseService?.process();
                        const processTime = timer.lap();
                        postMessage({
                            type: 'latency',
                            value: processTime,
                            index: chunkIndex++,
                        });

                        workletPort?.postMessage(
                            {
                                type: 'output',
                                output: outputChunk,
                            },
                            // [outputChunk.buffer], // TODO transfer?
                        );

                        // await new Promise<void>((resolve) => {
                        //   setTimeout(() => resolve(), 10);
                        // });

                        if (denoiseService?.hasEnoughInput()) {
                            nextToProcessId = setTimeout(processNext);
                        } else {
                            nextToProcessId = undefined;
                        }
                    });
                }
            }
            break;
        default:
            console.error('unknown type from WORKLET');
    }
}

type MainMessage =
    | { type: 'port'; port: MessagePort }
    | {
          type: 'init';
          wasm: ArrayBuffer;
          data: ArrayBuffer;
          options: {
              inputSizeUpperBound: number;
              inputSizeAlertBound: number;
              warmUpSteps: number;
          };
      }
    | { type: 'reset' }
    | { type: 'destroy' };

function onMainMessage(event: { data: MainMessage }) {
    switch (event.data.type) {
        case 'port': {
            workletPort = event.data.port;
            workletPort.onmessage = onWorkletMessage;
            sendState('got_worklet_port', timer.lap());
            break;
        }
        case 'init': {
            sendState('got_wasm_and_data', timer.lap());
            const { wasm, data, options } = event.data;
            const { inputSizeUpperBound, inputSizeAlertBound, warmUpSteps } = options;

            const spenchWasmModule = initSpenchWasmModule(wasm, data);
            sendState('wasm_runtime_initialized', timer.lap());

            denoiseService = createDenoiseService(spenchWasmModule, inputSizeUpperBound, inputSizeAlertBound);
            denoiseService.warmUp(warmUpSteps);
            sendState('spench_runtime_initialized', timer.lap());
            break;
        }
        case 'reset': {
            if (denoiseService) {
                denoiseService.resetContext();
                denoiseService.clearInput();
            }
            break;
        }
        case 'destroy': {
            if (denoiseService) {
                denoiseService.releaseResources();
            }
            break;
        }

        default:
            console.error('unknown type from MAIN');
    }
}

onmessage = onMainMessage;
