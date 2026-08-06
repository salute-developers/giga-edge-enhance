export default {
    roots: ['<rootDir>/src'],
    transform: {
        '^.+\\.tsx?$': [
            'ts-jest',
            {
                tsconfig: '<rootDir>/tsconfig.json',
                useESM: true,
            },
        ],
    },
    extensionsToTreatAsEsm: ['.ts', '.tsx'],
    testEnvironment: 'jsdom',
    testMatch: ['**/__test__/**/*.test.{j,t}s?(x)'],
    maxWorkers: '50%',
    testTimeout: 20000,
    detectOpenHandles: true,
};
