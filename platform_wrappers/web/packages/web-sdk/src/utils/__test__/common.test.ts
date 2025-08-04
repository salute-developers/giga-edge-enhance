import { copyArrayBuffer } from '../common';

describe('copyArrayBuffer', () => {
    it('copies an ArrayBuffer', () => {
        const first = new Float32Array([1, 2.3, 4, 5.67, 8, 9, 0]);
        const copied = copyArrayBuffer(first.buffer);
        const second = new Float32Array(copied);

        expect(first).not.toBe(second);
        expect(first).toEqual(second);
    });
});
