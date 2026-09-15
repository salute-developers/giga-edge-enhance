/* eslint-disable no-console */

import { CONTROLLER_MESSAGES, ControllerMessage, CustomPort } from './ipc';

export type LoggingFunc = (...data: unknown[]) => void;
export type Verbosity = 'verbose' | 'normal' | 'silent';

export type Logger = {
    debug: LoggingFunc;
    error: LoggingFunc;
    warn: LoggingFunc;
    info: LoggingFunc;
    verbosity: Verbosity;
};

export type LogLevel = keyof Omit<Logger, 'verbosity'>;

export type ExternalLogger = Partial<Omit<Logger, 'verbosity'>>;

const defaultLoggingFunc: LoggingFunc = (...data) => console.log('[GigaDenoiserSDK]', ...data);

function getActiveLogLevels(verbosity: Verbosity): LogLevel[] {
    const levels: LogLevel[] = ['error'];

    if (['normal', 'verbose'].includes(verbosity)) levels.push('warn', 'info');

    if (verbosity === 'verbose') levels.push('debug');

    return levels;
}

// TODO: own logging (track errors)
function createLogger(verbosity: Verbosity, externalLogger: ExternalLogger = {}): Logger {
    const activeLevels = getActiveLogLevels(verbosity);
    const factory = (level: LogLevel): LoggingFunc => {
        const ext = externalLogger[level];

        if (typeof ext === 'function') return ext;

        if (!activeLevels.includes(level)) return () => {};

        return (...data) => defaultLoggingFunc(`[${level}]`, ...data);
    };

    return (['error', 'info', 'warn', 'debug'] as const).reduce(
        (acc, lev) => {
            acc[lev] = factory(lev);

            return acc;
        },
        { verbosity } as Logger,
    );
}

export function getWorkerLogger(
    port: Pick<CustomPort<any, ControllerMessage>, 'postMessage'>,
    verbosity: Verbosity,
    prefix = '',
): Logger {
    const factory = (level: LogLevel): LoggingFunc => {
        return (...data) =>
            port.postMessage({
                type: CONTROLLER_MESSAGES.LOG,
                level,
                data: prefix ? [prefix, ...data] : data,
            });
    };

    return (['error', 'info', 'warn', 'debug'] as const).reduce(
        (acc, lev) => {
            acc[lev] = factory(lev);

            return acc;
        },
        { verbosity } as Logger,
    );
}

export default createLogger;
