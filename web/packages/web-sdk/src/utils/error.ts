/**
 * Base error type used throughout the GigaEdgeSpench SDK.
 *
 * @extends Error
 *
 * @remarks
 * The constructor can be called in two ways:
 * 1. `new GigaEdgeSpenchError(error: Error)` – wraps an existing error,
 *    preserving its stack trace and setting `cause` to the original error.
 * 2. `new GigaEdgeSpenchError(message: string)` – creates a new error with the
 *    supplied message.
 *
 * The `name` getter returns the actual constructor name (`GigaEdgeSpenchError`
 * or any subclass) and the `message` getter guarantees a non‑empty
 * string by falling back to `'Unknown error'`.
 */
class GigaEdgeSpenchError extends Error {
    /** @type {Error | undefined} */
    cause: Error | undefined;

    /** @param {unknown} error */
    constructor(error: unknown);

    /** @param {string} [message] */
    constructor(message?: string);

    /** @internal */
    constructor(param?: string | Error) {
        const isError = param instanceof Error;
        super(isError ? param.message : param);

        if (isError) {
            this.stack = param.stack;
            this.cause = param;
        }
    }

    /** @override */
    override get name() {
        return 'GigaEdgeSpenchError';
    }

    /** @override */
    override get message(): string {
        return super.message || 'Unknown error';
    }
}

/** @extends GigaEdgeSpenchError */
class AssertionError extends GigaEdgeSpenchError {
    override get name() {
        return 'AssertionError';
    }
}

/**
 * Asserts a condition and throws an {@link AssertionError} if the assertion fails.
 *
 * @param {any} condition - The value to assert. Must be truthy for the assertion to pass.
 * @param {string} [message] - Optional error message for the assertion failure.
 * @returns {asserts condition} - If the function returns, the condition is guaranteed to be truthy.
 *
 * @throws {AssertionError} When `condition` evaluates to a falsy value.
 */
export function assert(condition: any, message?: string): asserts condition {
    if (!condition) {
        throw new AssertionError(message);
    }
}

export default GigaEdgeSpenchError;
