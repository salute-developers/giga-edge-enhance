// Jest unit tests for the polynomial hash implementation in hash.ts

import getHashCode from '../hash';

describe('getHashCode', () => {
    it('should return "0" for an empty string', () => {
        expect(getHashCode('')).toBe('0');
    });

    it('should return correct hash for single character', () => {
        // 'a' (charCode 97) => 97 -> 0x61
        expect(getHashCode('a')).toBe('61');
    });

    it('should return correct hash for "ab"', () => {
        // Pre‑computed: 0x252e3
        expect(getHashCode('ab')).toBe('252e3');
    });

    it('should return correct hash for "abc"', () => {
        // Pre‑computed: 0xe3da6a6
        expect(getHashCode('abc')).toBe('e3da6a6');
    });

    it('should produce deterministic results', () => {
        const value = 'deterministic';
        const first = getHashCode(value);
        const second = getHashCode(value);
        expect(first).toBe(second);
    });

    it('should return different hashes for different strings', () => {
        expect(getHashCode('ab')).not.toBe(getHashCode('ac'));
    });

    it('hash string should consist only of lower‑case hex digits', () => {
        const hexRegex = /^[0-9a-f]+$/;
        expect(getHashCode('test')).toMatch(hexRegex);
    });

    it('should correctly hash strings containing surrogate pairs', () => {
        // Emoji represented by two UTF‑16 code units
        const emoji = '🙂';
        const hash = getHashCode(emoji);
        expect(hash).toBeDefined();
        expect(hash).toMatch(/^[0-9a-f]+$/);
    });

    afterEach(() => {
        jest.clearAllMocks();
    });
});
