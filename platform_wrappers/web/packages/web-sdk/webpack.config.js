const path = require('node:path');

const baseConfig = {
    mode: process.env.NODE_ENV || 'development',
    devtool: 'source-map',
    resolve: {
        extensions: ['.ts', '.js'],
        alias: {
            'wasm-wrapper': path.resolve(__dirname, 'lib/Spench'),
        },
        fallback: {
            crypto: false,
        },
    },
    module: {
        rules: [
            {
                loader: require.resolve('babel-loader'),
                test: /\.(js|ts)$/,
                exclude: /node_modules/,
                options: {
                    presets: [
                        [
                            '@babel/preset-env',
                            {
                                useBuiltIns: 'entry',
                                corejs: 3,
                                exclude: ['transform-typeof-symbol'],
                                // Minimum targets for AudioWorklet
                                targets: {
                                    chrome: '66',
                                    edge: '79',
                                    safari: '14.1',
                                    firefox: '76',
                                    opera: '53',
                                    android: '128',
                                    ios: '14.5',
                                    samsung: '9.2',
                                },
                            },
                        ],
                        '@babel/preset-typescript',
                    ],
                },
            },
        ],
    },
    output: {
        path: path.resolve(__dirname, 'dist/workers'),
        filename: '[name].min.js',
    },
};

const spenchWorkerConfig = {
    ...baseConfig,
    entry: {
        'spench-worker': './src/workers/worker/index.ts',
    },
    output: {
        ...baseConfig.output,
        globalObject: 'this',
    },
};

const spenchWorkletConfig = {
    ...baseConfig,
    entry: {
        'spench-worklet': './src/workers/worklet/index.ts',
    },
    output: {
        ...baseConfig.output,
        globalObject: 'AudioWorkletGlobalScope',
    },
};

const normalizerConfig = {
    ...baseConfig,
    entry: {
        'normalizer-worklet': './src/workers/normalizer/index.ts',
    },
    output: {
        ...baseConfig.output,
        globalObject: 'AudioWorkletGlobalScope',
    },
};

module.exports = [spenchWorkletConfig, spenchWorkerConfig, normalizerConfig];
