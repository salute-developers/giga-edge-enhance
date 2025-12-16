class SpenchError extends Error {
    cause: Error | undefined;

    constructor(error: unknown);

    constructor(message?: string);

    constructor(param?: string | Error) {
        const isError = param instanceof Error;
        super(isError ? param.message : param);

        if (isError) {
            this.stack = param.stack;
            this.cause = param;
        }
    }

    override get name() {
        return this.constructor.name;
    }

    override get message(): string {
        return super.message || 'Unknown error';
    }
}

class AssertionError extends SpenchError {}

export function assert(condition: any, message?: string): asserts condition {
    if (!condition) {
        throw new AssertionError(message);
    }
}

export default SpenchError;
