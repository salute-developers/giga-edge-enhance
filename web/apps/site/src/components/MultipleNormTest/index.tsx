import { type GigaEdgeEnhanceAudioNode, GigaEdgeEnhanceSDK } from '@gigaedge/enhance-web-sdk';
import { useCallback, useRef, useState } from 'react';

function MultipleNormTest() {
    const [isProcessingMultipleNorm, setIsProcessingMultipleNorm] = useState(false);
    const [multipleNormResult, setMultipleNormResult] = useState<null | boolean>(null);
    const [multinormProgress, setMultinormProgress] = useState<number>(0);

    const sdkRef = useRef<GigaEdgeEnhanceSDK>(null);

    const checkMultipleNormalizerNodes = useCallback(async () => {
        setIsProcessingMultipleNorm(true);
        setMultipleNormResult(null);

        try {
            let sdk;

            if (!sdkRef.current) {
                const ctx = new AudioContext();

                sdk = GigaEdgeEnhanceSDK.create(ctx, {
                    loggerVerbosity: 'silent',
                    sendMetric: (metric, value) => console.log(metric, value),
                    settings: {
                        dspOnAudioThread: true,
                    },
                });
                sdkRef.current = sdk;
            } else {
                sdk = sdkRef.current;
            }

            let nodes = [] as GigaEdgeEnhanceAudioNode[];

            for (let i = 0; i < 50; ++i) {
                console.log(`Multinorm test iteration #${i + 1}`);
                nodes = [];

                for (let j = 0; j < 50; ++j) {
                    setMultinormProgress(Math.round((i * 50 + j + 1) / 25));
                    const gigaEdgeEnhanceNode = await sdk.createAudioNode({ normalization: true });

                    if (gigaEdgeEnhanceNode instanceof Error) {
                        throw gigaEdgeEnhanceNode;
                    }

                    nodes.push(gigaEdgeEnhanceNode);
                }

                for (const node of nodes) {
                    node.destroy();
                }
            }

            setIsProcessingMultipleNorm(false);
            setMultipleNormResult(true);
        } catch {
            setIsProcessingMultipleNorm(false);
            setMultipleNormResult(false);
        }
    }, []);

    return (
        <div>
            <button
                data-testid="startMultinormCheckButton"
                disabled={isProcessingMultipleNorm}
                onClick={checkMultipleNormalizerNodes}
            >
                Check Multiple Normalizer Nodes
            </button>
            {isProcessingMultipleNorm && (
                <div data-testid="multinormProgressStatus" style={{ color: 'orange' }}>
                    Processing... {multinormProgress}%
                </div>
            )}
            {!isProcessingMultipleNorm && multipleNormResult !== null && (
                <div
                    data-testid={multipleNormResult ? 'multinormOkStatus' : 'multinormFailStatus'}
                    style={{ color: multipleNormResult ? 'green' : 'red' }}
                >
                    {multipleNormResult ? 'OK' : 'FAIL'}
                </div>
            )}
        </div>
    );
}

export default MultipleNormTest;
