const path = require('node:path');

const baseConfigFactory = (mode = 'development') => {
    return {
        mode,
        devtool: mode === 'production' ? 'source-map' : 'eval-source-map',
        resolve: {
            extensions: ['.ts', '.js'],
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
                                    /**
                                     *  TODO Maybe change to:
                                     *  fully supports audio-api and fully supports wasm and fully supports webworkers
                                     **/
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
};

const gigaEdgeEnhanceWorkerConfigFactory = (mode) => {
    const baseConfig = baseConfigFactory(mode);

    return {
        ...baseConfig,
        entry: {
            'gigaedge-enhance-worker': './src/workers/worker/index.ts',
        },
        output: {
            ...baseConfig.output,
            globalObject: 'this',
        },
    };
};

const gigaEdgeEnhanceWorkletConfigFactory = (mode) => {
    const baseConfig = baseConfigFactory(mode);

    return {
        ...baseConfig,
        entry: {
            'gigaedge-enhance-worklet': './src/workers/worklet/index.ts',
        },
        output: {
            ...baseConfig.output,
            globalObject: 'AudioWorkletGlobalScope',
        },
    };
};

const gigaEdgeEnhanceWorkletDspConfigFactory = (mode) => {
    const baseConfig = baseConfigFactory(mode);

    return {
        ...baseConfig,
        entry: {
            'gigaedge-enhance-worklet-dsp': './src/workers/worklet/dsp.ts',
        },
        output: {
            ...baseConfig.output,
            globalObject: 'AudioWorkletGlobalScope',
        },
    };
};

module.exports = (env, argv) => {
    const { mode } = argv;

    return [gigaEdgeEnhanceWorkletConfigFactory(mode), gigaEdgeEnhanceWorkerConfigFactory(mode), gigaEdgeEnhanceWorkletDspConfigFactory(mode)];
};
