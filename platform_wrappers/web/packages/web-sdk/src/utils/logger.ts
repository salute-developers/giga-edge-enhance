/* eslint-disable no-console */

import { CONTROLLER_MESSAGES, ControllerMessage, CustomPort } from './ipc';

type LoggingFunc = (...data: unknown[]) => void;

export type Logger = {
    debug: LoggingFunc;
    error: LoggingFunc;
    warn: LoggingFunc;
};

export type ExternalLogger = Partial<Logger>;

const defaultLoggingFunc: LoggingFunc = (...data) => console.log('[SpenchSDK]', ...data);
const noLogging: LoggingFunc = () => {};

// TODO: own logging (track errors)
function createLogger(isDebug: boolean, externalLogger: ExternalLogger = {}): Logger {
    const fallbackLogger = isDebug ? defaultLoggingFunc : noLogging;
    const { debug, error, warn } = externalLogger;

    return {
        error(...data) {
            if (error) error(...data);
            else defaultLoggingFunc('[error]', ...data);
        },
        warn(...data) {
            if (warn) warn(...data);
            else fallbackLogger('[warn]', ...data);
        },
        debug(...data) {
            if (debug) debug(...data);
            else fallbackLogger('[debug]', ...data);
        },
    };
}

export function getWorkerLogger(port: Pick<CustomPort<any, ControllerMessage>, 'postMessage'>, prefix = ''): Logger {
    const factory =
        (level: keyof Logger): LoggingFunc =>
        (...data) => {
            port.postMessage({
                type: CONTROLLER_MESSAGES.LOG,
                level,
                data: prefix ? [prefix, ...data] : data,
            });
        };

    return (['error', 'warn', 'debug'] as const).reduce((acc, lev) => {
        acc[lev] = factory(lev);

        return acc;
    }, {} as Logger);
}

export default createLogger;
