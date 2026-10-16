import { type Features as RawFeatures, GigaEdgeSpenchSDK } from '@gigaedge/spench-web-sdk';
import { useCallback, useEffect, useRef, useState, type RefObject } from 'react';
import Statistics from '../Statistics';
import benchmarkFile from './sample.wav';
import { getEffectiveSettings, type DemoSettings } from '../../settings';

type Result = {
    processTime: number[];
    afterBlob?: Blob;
    beforeBlob?: Blob;
};

const recordingFormats = [
    { mimeType: 'audio/webm;codecs=opus', extension: 'webm' },
    { mimeType: 'audio/mp4;codecs=mp4a.40.2', extension: 'm4a' },
    { mimeType: 'audio/mp4', extension: 'm4a' },
] as const;

function getRecordingFormat() {
    const format = recordingFormats.find(({ mimeType }) => MediaRecorder.isTypeSupported(mimeType));

    if (!format) {
        throw new Error('This browser does not support WebM/Opus or MP4/AAC audio recording.');
    }

    return format;
}

function getRecordingExtension(blob: Blob) {
    return blob.type.startsWith('audio/mp4') ? 'm4a' : 'webm';
}

type Models = {
    noiseCancellation: { fast8: URL; quality48: URL; hybrid48: URL; custom: ArrayBuffer };
};
export type Features = RawFeatures<Models>;

async function processAudio(
    fileBuffer: ArrayBuffer,
    transport: 'message' | 'shared',
    features: Features,
    settings: DemoSettings,
    setFeaturesRef: RefObject<((f: Features) => Promise<void | Error>) | null>,
    stopRef: RefObject<(() => void) | null>,
    resetRef: RefObject<(() => void) | null>,
    onLowPerformance: () => void,
    setCurrentMeanProcessTime: (t: number) => void,
    sampleRate?: number,
    customDenoiseModel?: ArrayBuffer,
    instances?: number,
    context?: AudioContext,
    captureOutput = true,
): Promise<Result> {
    const ctx = context || new AudioContext({ sampleRate });
    const inputBuffer = await ctx.decodeAudioData(fileBuffer);

    if (ctx.state !== 'running') {
        console.log('Resuming context');
        await Promise.race([ctx.resume(), new Promise((res) => setTimeout(res, 1000))]);

        if ((ctx.state as AudioContextState) === 'running') {
            console.log('context resumed');
        } else {
            throw new Error('Context not running');
        }
    }

    const processTime = [] as number[];
    const effectiveSettings = getEffectiveSettings(settings);
    const processingChunkSeconds = Math.max(
        0.016,
        Math.min(effectiveSettings.maxProcessingChunkSeconds, effectiveSettings.minLatency),
    );
    const drainMilliseconds = Math.ceil(
        (effectiveSettings.minLatency +
            Math.max(effectiveSettings.lowPerformanceMaxRefillTimes - 1, 0) *
                effectiveSettings.latencyIncreaseStep +
            processingChunkSeconds * 2) *
            1000,
    );

    // Create nodes
    const srcNode = ctx.createBufferSource();
    const destNode = ctx.createMediaStreamDestination();
    srcNode.buffer = inputBuffer; // monoBuffer;

    const sdk = GigaEdgeSpenchSDK.create(ctx, {
        loggerVerbosity: 'verbose',
        settings: {
            ...settings,
            sharedMemoryEnabled: transport === 'shared',
        },
        models: {
            noiseCancellation: {
                custom: customDenoiseModel!,
            },
        },
        sendMetric: (metric, value) => {
            console.log(metric, value);
            if (metric === 'frameProcessTimeRaw') {
                const val = value as number[];
                processTime.push(...val);
                let s = 0;

                for (const v of val) {
                    s += v;
                }

                setCurrentMeanProcessTime(s / val.length);
            }
        },
    });

    const gigaEdgeSpenchNode = await sdk.createAudioNode(features);

    if (gigaEdgeSpenchNode instanceof Error) {
        if (ctx.state !== 'closed' && ctx !== context) await ctx.close();
        throw gigaEdgeSpenchNode;
    }
    const additionalNodes: (typeof gigaEdgeSpenchNode)[] = [];

    try {
        let fail: (() => void) | undefined;
        gigaEdgeSpenchNode.addEventListener('low_performance', onLowPerformance);
        gigaEdgeSpenchNode.addEventListener('destroyed', () => {
            fail?.();
        });
        const setFeatures = (f: Features) => {
            for (const node of additionalNodes) {
                node.setFeatures(f);
            }

            return gigaEdgeSpenchNode.setFeatures(f);
        };
        setFeaturesRef.current = setFeatures;

        console.log('node ready');

        // Connect nodes
        srcNode.connect(gigaEdgeSpenchNode);
        gigaEdgeSpenchNode.connect(destNode);

        const recordingFormat = captureOutput ? getRecordingFormat() : undefined;
        const mediaRecorder = recordingFormat
            ? new MediaRecorder(destNode.stream, { mimeType: recordingFormat.mimeType })
            : undefined;

        const chunks: Blob[] = [];

        let finish: (() => void) | undefined;
        const processingStopped = new Promise<void>((resolve, reject) => {
            fail = () => reject();
            finish = () => {
                fail = undefined;
                resolve();
            };
        });

        if (mediaRecorder) {
            console.log('Recording with MediaRecorder');
            mediaRecorder.ondataavailable = (event) => {
                console.log('MediaRecorder ondataavailable');
                if (event.data.size > 0) chunks.push(event.data);
            };
            mediaRecorder.onstop = () => {
                console.log('MediaRecorder onstop');
                finish?.();
            };
            console.log('Starting MediaRecorder');
            mediaRecorder.start();
        }
        console.log('Starting source, context state', ctx.state);
        srcNode.start();

        // Stop recording when audio finishes
        srcNode.onended = () => {
            console.log('MediaRecorder onended');
            window.setTimeout(() => {
                if (mediaRecorder && mediaRecorder.state !== 'inactive') mediaRecorder.stop();
                else finish?.();
            }, drainMilliseconds);
        };

        let manuallyStopped = false;

        const stop = () => {
            console.log('Stop processing');
            manuallyStopped = true;
            if (mediaRecorder && mediaRecorder.state !== 'inactive') mediaRecorder.stop();
            else finish?.();
        };

        stopRef.current = stop;
        const reset = () => gigaEdgeSpenchNode.reset();
        resetRef.current = reset;

        console.log('Waiting for processing to finish');

        const totalAdditionalInstances = (instances || 1) - 1;

        if (totalAdditionalInstances > 0) {
            console.log(`Creating ${totalAdditionalInstances} additional instances`);

            for (let i = 0; i < totalAdditionalInstances; ++i) {
                const node = await sdk.createAudioNode(features);

                if (node instanceof Error) {
                    throw node;
                }

                additionalNodes.push(node);

                node.addEventListener('low_performance', onLowPerformance);
                node.addEventListener('destroyed', () => {
                    fail?.();
                });

                console.log('additional node ready');

                const dest = ctx.createMediaStreamDestination();

                // Connect nodes
                srcNode.connect(node);
                node.connect(dest);
                console.log('additional node connected');
            }
        }

        await processingStopped;

        if (manuallyStopped) {
            console.log('Stopped manually');
            return { processTime };
        }

        console.log('Processing finished');

        return {
            processTime,
            afterBlob: recordingFormat ? new Blob(chunks, { type: recordingFormat.mimeType }) : undefined,
        };
    } finally {
        stopRef.current = null;
        resetRef.current = null;
        setFeaturesRef.current = null;

        // Disconnect nodes
        if (srcNode) srcNode.disconnect();
        if (destNode) destNode.disconnect();
        if (gigaEdgeSpenchNode) gigaEdgeSpenchNode.disconnect();

        for (const node of additionalNodes) {
            node.disconnect();
            node.destroy();
        }

        gigaEdgeSpenchNode.destroy();

        if (ctx.state !== 'closed' && ctx !== context) await ctx.close();
    }
}

function saveRecording(name: string, blob: Blob) {
    console.log('Saving ', name);
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.style.visibility = 'hidden';
    link.style.position = 'absolute';
    link.download = `${name}.${getRecordingExtension(blob)}`;
    console.log('Inserting link');
    document.body.appendChild(link);
    link.click();

    setTimeout(() => {
        document.body.removeChild(link);
        URL.revokeObjectURL(url);
    }, 10000);
}

async function processLiveDemo(
    transport: 'message' | 'shared',
    features: Features,
    settings: DemoSettings,
    setFeaturesRef: RefObject<((f: Features) => Promise<void | Error>) | null>,
    stopRef: RefObject<(() => void) | null>,
    resetRef: RefObject<(() => void) | null>,
    setCurrentMeanProcessTime: (t: number) => void,
    onLowPerformance: () => void,
    sampleRate?: number,
    customDenoiseModel?: ArrayBuffer,
    delayTime = 1,
): Promise<Result> {
    const micStream = await navigator.mediaDevices.getUserMedia({
        audio: { noiseSuppression: true, autoGainControl: true, echoCancellation: false },
    });
    const processTime = [] as number[];
    let ctx: AudioContext | undefined;
    let sourceNode: MediaStreamAudioSourceNode | undefined;
    let destNode: MediaStreamAudioDestinationNode | undefined;
    let delayNode: DelayNode | undefined;
    let gigaEdgeSpenchNode: Awaited<ReturnType<ReturnType<typeof GigaEdgeSpenchSDK.create>['createAudioNode']>> | undefined;
    let recorderBefore: MediaRecorder | undefined;
    let recorderAfter: MediaRecorder | undefined;

    try {
        ctx = new AudioContext({ sampleRate });
        delayNode = ctx.createDelay();
        delayNode.delayTime.value = Math.max(0, delayTime);

        sourceNode = ctx.createMediaStreamSource(micStream);
        destNode = ctx.createMediaStreamDestination();

        const sdk = GigaEdgeSpenchSDK.create(ctx, {
            loggerVerbosity: 'verbose',
            settings: {
                ...settings,
                sharedMemoryEnabled: transport === 'shared',
            },
            models: {
                noiseCancellation: {
                    custom: customDenoiseModel!,
                },
            },
            sendMetric: (metric, value) => {
                console.log(metric, value);
                if (metric === 'frameProcessTimeRaw') {
                    const val = value as number[];
                    processTime.push(...val);
                    let s = 0;

                    for (const v of val) {
                        s += v;
                    }

                    setCurrentMeanProcessTime(s / val.length);
                }
            },
        });

        gigaEdgeSpenchNode = await sdk.createAudioNode(features);

        if (gigaEdgeSpenchNode instanceof Error) {
            throw gigaEdgeSpenchNode;
        }
        const activeGigaEdgeSpenchNode = gigaEdgeSpenchNode;

        activeGigaEdgeSpenchNode.addEventListener('low_performance', onLowPerformance);
        setFeaturesRef.current = (f) => activeGigaEdgeSpenchNode.setFeatures(f);

        console.log('node ready');

        sourceNode.connect(activeGigaEdgeSpenchNode);
        activeGigaEdgeSpenchNode.connect(destNode);
        activeGigaEdgeSpenchNode.connect(delayNode);
        delayNode.connect(ctx.destination);

        const recordingFormat = getRecordingFormat();
        recorderBefore = new MediaRecorder(micStream, {
            mimeType: recordingFormat.mimeType,
        });
        const chunksBefore = [] as Blob[];
        recorderBefore.ondataavailable = (event) => {
            console.log('recorderBefore ondataavailable');
            if (event.data.size > 0) chunksBefore.push(event.data);
        };
        const recordingBeforeStopped = new Promise<void>((resolve) => {
            recorderBefore!.onstop = () => {
                console.log('recorderBefore onstop');
                resolve();
            };
        });

        recorderAfter = new MediaRecorder(destNode.stream, {
            mimeType: recordingFormat.mimeType,
        });
        const chunksAfter = [] as Blob[];
        recorderAfter.ondataavailable = (event) => {
            console.log('recorderAfter ondataavailable');
            if (event.data.size > 0) chunksAfter.push(event.data);
        };
        const recordingAfterStopped = new Promise<void>((resolve) => {
            recorderAfter!.onstop = () => {
                console.log('recorderAfter onstop');
                resolve();
            };
        });

        if (ctx.state !== 'running') {
            console.log('Resuming context');
            await ctx.resume();
            console.log('context resumed');
        }

        console.log('Starting MediaRecorder');
        recorderBefore.start();
        recorderAfter.start();

        const stop = () => {
            console.log('Stop MediaRecorders');
            if (recorderBefore?.state !== 'inactive') recorderBefore?.stop();
            if (recorderAfter?.state !== 'inactive') recorderAfter?.stop();
        };

        stopRef.current = stop;
        resetRef.current = () => activeGigaEdgeSpenchNode.reset();

        console.log('Waiting MediaRecorders');
        await Promise.all([recordingBeforeStopped, recordingAfterStopped]);

        return {
            processTime,
            beforeBlob: new Blob(chunksBefore, { type: recordingFormat.mimeType }),
            afterBlob: new Blob(chunksAfter, { type: recordingFormat.mimeType }),
        };
    } finally {
        stopRef.current = null;
        resetRef.current = null;
        setFeaturesRef.current = null;

        if (recorderBefore?.state !== 'inactive') recorderBefore?.stop();
        if (recorderAfter?.state !== 'inactive') recorderAfter?.stop();

        sourceNode?.disconnect();
        destNode?.disconnect();
        delayNode?.disconnect();
        if (gigaEdgeSpenchNode && !(gigaEdgeSpenchNode instanceof Error)) {
            gigaEdgeSpenchNode.disconnect();
            gigaEdgeSpenchNode.destroy();
        }

        if (ctx?.state !== 'closed') await ctx?.close();
        micStream.getTracks().forEach((track) => track.stop());
    }
}

type Props = {
    features: Features;
    setFeaturesRef: React.RefObject<((f: Features) => Promise<void | Error>) | null>;
    sampleRate?: number;
    settings: DemoSettings;
    customDenoiseModel?: ArrayBuffer;
    onProcessingChange?: (isProcessing: boolean) => void;
};

type RunnerMode = 'file' | 'live' | 'benchmark';

type ProcessedResult = {
    name: string;
    originalUrl: string;
    processedUrl: string;
};

const Runner: React.FC<Props> = ({
    features,
    sampleRate,
    setFeaturesRef,
    settings,
    customDenoiseModel,
    onProcessingChange,
}) => {
    const inputRef = useRef<HTMLInputElement>(null);
    const stopRef = useRef<(() => void) | null>(null);
    const resetRef = useRef<(() => void) | null>(null);
    const [mode, setMode] = useState<RunnerMode>('file');
    const [isProcessingTrack, setIsProcessingTrack] = useState(false);
    const [isProcessingLiveDemo, setIsProcessingLiveDemo] = useState(false);
    const [shouldSaveRecording, setShouldSaveRecording] = useState(false);
    const [isProcessingLowPerformance, setIsProcessingLowPerformance] = useState(false);
    const [trackProcessResult, setTrackProcessResult] = useState<boolean | null>(null);
    const [processTime, setProcessTime] = useState<number | null>(null);
    const [processTimeRaw, setProcessTimeRaw] = useState<number[]>([]);
    const [processedResult, setProcessedResult] = useState<ProcessedResult | null>(null);
    const [instances, setInstances] = useState<number>(1);
    const transport: 'message' | 'shared' =
        getEffectiveSettings(settings).sharedMemoryEnabled && window.crossOriginIsolated ? 'shared' : 'message';

    useEffect(() => {
        onProcessingChange?.(isProcessingTrack || isProcessingLiveDemo);
    }, [isProcessingLiveDemo, isProcessingTrack, onProcessingChange]);

    useEffect(() => {
        return () => {
            if (processedResult) {
                URL.revokeObjectURL(processedResult.originalUrl);
                URL.revokeObjectURL(processedResult.processedUrl);
            }
        };
    }, [processedResult]);

    const clear = useCallback(() => {
        if (inputRef.current) {
            inputRef.current.value = '';
        }
    }, [inputRef]);

    const onLowPerformance = useCallback(() => {
        setIsProcessingLowPerformance(true);
    }, []);

    const handleBenchmark = useCallback(async () => {
        setIsProcessingTrack(true);
        setIsProcessingLowPerformance(false);
        setTrackProcessResult(null);

        try {
            const res = await processAudio(
                await (await fetch(benchmarkFile)).arrayBuffer(),
                transport,
                features,
                settings,
                setFeaturesRef,
                stopRef,
                resetRef,
                onLowPerformance,
                setProcessTime,
                sampleRate,
                customDenoiseModel,
                instances,
                undefined,
                false,
            );

            setProcessTimeRaw(res.processTime);

            setIsProcessingTrack(false);
            setTrackProcessResult(true);
        } catch (e) {
            console.log('Error while processing:', e);
            setIsProcessingTrack(false);
            setTrackProcessResult(false);
        }
    }, [
        customDenoiseModel,
        features,
        instances,
        onLowPerformance,
        sampleRate,
        settings,
        setFeaturesRef,
        transport,
    ]);

    const handleFiles = useCallback(async () => {
        if (!inputRef.current || !inputRef.current.files) {
            return;
        }

        setTrackProcessResult(null);
        let context: AudioContext | undefined;

        try {
            const { files } = inputRef.current;
            const [file] = Array.from(files);

            if (!file) {
                return;
            }

            context = new AudioContext({ sampleRate });

            if (context.state !== 'running') {
                console.log('Resuming context');
                await Promise.race([context.resume(), new Promise((res) => setTimeout(res, 1000))]);

                if ((context.state as AudioContextState) === 'running') {
                    console.log('context resumed');
                } else {
                    console.log('Context not running');
                }
            }

            setIsProcessingTrack(true);
            setIsProcessingLowPerformance(false);
            setProcessedResult(null);

            const res = await processAudio(
                await file.arrayBuffer(),
                transport,
                features,
                settings,
                setFeaturesRef,
                stopRef,
                resetRef,
                onLowPerformance,
                setProcessTime,
                sampleRate,
                customDenoiseModel,
                1,
                context,
            );

            if (!res.afterBlob) {
                throw new Error('Processing finished without an output recording.');
            }

            setProcessedResult({
                name: `gigaedge-spench-processed-${file.name}.${getRecordingExtension(res.afterBlob)}`,
                originalUrl: URL.createObjectURL(file),
                processedUrl: URL.createObjectURL(res.afterBlob),
            });
            setProcessTimeRaw(res.processTime);

            clear();

            setIsProcessingTrack(false);
            setTrackProcessResult(true);
        } catch (error) {
            console.log('Error while processing:', error);
            setIsProcessingTrack(false);
            setTrackProcessResult(false);
        } finally {
            if (context && context.state !== 'closed') {
                await context.close();
            }
        }
    }, [
        clear,
        customDenoiseModel,
        features,
        onLowPerformance,
        sampleRate,
        settings,
        setFeaturesRef,
        transport,
    ]);

    const handleStartLiveDemo = useCallback(async () => {
        setIsProcessingLiveDemo(true);
        setIsProcessingLowPerformance(false);
        setTrackProcessResult(null);

        try {
            const res = await processLiveDemo(
                transport,
                features,
                settings,
                setFeaturesRef,
                stopRef,
                resetRef,
                setProcessTime,
                onLowPerformance,
                sampleRate,
                customDenoiseModel,
            );

            if (shouldSaveRecording) {
                if (res.beforeBlob) {
                    saveRecording('gigaedge-spench-recorded-original', res.beforeBlob);
                }

                if (res.afterBlob) {
                    saveRecording('gigaedge-spench-recorded-processed', res.afterBlob);
                }
            }
            setProcessTimeRaw(res.processTime);

            setIsProcessingLiveDemo(false);
        } catch (e) {
            console.log('Error while processing:', e);

            setIsProcessingLiveDemo(false);
            setTrackProcessResult(false);
        }
    }, [
        customDenoiseModel,
        features,
        onLowPerformance,
        sampleRate,
        settings,
        setFeaturesRef,
        shouldSaveRecording,
        transport,
    ]);

    return (
        <div className="runner">
            <div className="panelHeader runnerHeader">
                <div>
                    <p className="panelKicker">Listen and compare</p>
                    <h2>Run the pipeline</h2>
                </div>
                <span className={`resultBadge ${trackProcessResult === false ? 'resultBadgeError' : ''}`}>
                    {isProcessingTrack || isProcessingLiveDemo ? 'Processing' : trackProcessResult === false ? 'Needs attention' : 'Ready'}
                </span>
            </div>

            <div className="modeTabs" role="tablist" aria-label="Processing mode">
                {(['file', 'live', 'benchmark'] as RunnerMode[]).map((nextMode) => (
                    <button
                        key={nextMode}
                        type="button"
                        role="tab"
                        aria-selected={mode === nextMode}
                        className={mode === nextMode ? 'modeTab modeTabActive' : 'modeTab'}
                        data-testid={`${nextMode}ModeTab`}
                        onClick={() => setMode(nextMode)}
                    >
                        {nextMode === 'file' ? 'Audio file' : nextMode === 'live' ? 'Microphone' : 'Benchmark'}
                    </button>
                ))}
            </div>

            {mode === 'file' && (
                <section className="modePanel" role="tabpanel">
                    <label className="uploadArea" htmlFor="fileInput">
                        <span className="uploadIcon" aria-hidden="true">↑</span>
                        <strong>Choose an audio file</strong>
                        <small>Processing starts locally as soon as the file is selected.</small>
                        <input
                            data-testid="processingInput"
                            disabled={isProcessingTrack || isProcessingLiveDemo}
                            id="fileInput"
                            type="file"
                            ref={inputRef}
                            accept="audio/*,video/*"
                            multiple={false}
                            onClick={clear}
                            onChange={handleFiles}
                        />
                    </label>
                </section>
            )}

            {mode === 'live' && (
                <section className="modePanel livePanel" role="tabpanel">
                    <p>Use headphones to avoid acoustic feedback. Microphone audio stays in this browser tab.</p>
                    <button className="primaryAction" disabled={isProcessingTrack} onClick={!isProcessingLiveDemo ? handleStartLiveDemo : () => stopRef.current?.()}>
                        {isProcessingLiveDemo ? 'Stop microphone' : 'Start microphone'}
                    </button>
                    {isProcessingLiveDemo && <button onClick={() => resetRef.current?.()}>Reset pipeline</button>}
                    <label className="saveToggle">
                        <input checked={shouldSaveRecording} onChange={() => setShouldSaveRecording(!shouldSaveRecording)} type="checkbox" />
                        Save original and processed recordings when stopped
                    </label>
                </section>
            )}

            {mode === 'benchmark' && (
                <section className="modePanel benchmarkPanel" role="tabpanel">
                    <label>
                        Simultaneous instances
                        <input value={instances} max={5} min={1} data-testid="benchmarkInstancesCount" onChange={(e) => setInstances(Number(e.target.value))} type="number" />
                    </label>
                    <button className="primaryAction" data-testid="benchmarkButton" disabled={isProcessingTrack || isProcessingLiveDemo} onClick={handleBenchmark}>
                        Run benchmark
                    </button>
                </section>
            )}

            {isProcessingTrack && (
                <div className="statusCard statusCardWorking" data-testid="processingProgressStatus" role="status">
                    <span><strong>Processing audio</strong><small>Preparing the model and running the selected pipeline.</small></span>
                    <div><button onClick={() => stopRef.current?.()}>Stop</button><button onClick={() => resetRef.current?.()}>Reset</button></div>
                </div>
            )}
            {isProcessingLowPerformance && <div className="statusCard statusCardWarning" data-testid="processingLowPerformance" role="alert">The browser is falling behind the real-time audio budget.</div>}
            {!isProcessingTrack && trackProcessResult !== null && (
                <div className={`statusCard ${trackProcessResult ? 'statusCardSuccess' : 'statusCardError'}`} data-testid={trackProcessResult ? 'processingOkStatus' : 'processingFailStatus'} role="status">
                    <strong>{trackProcessResult ? 'Processing complete' : 'Processing failed'}</strong>
                    <small>{trackProcessResult ? 'The result is ready to review.' : 'Check the selected model, sample rate, and browser console.'}</small>
                </div>
            )}

            {processedResult && (
                <section className="audioComparison" aria-label="Original and processed audio">
                    <div className="audioCard"><span>Original</span><audio controls src={processedResult.originalUrl} /></div>
                    <div className="audioCard audioCardProcessed"><span>Processed</span><audio controls src={processedResult.processedUrl} /></div>
                    <a className="downloadAction" data-testid="downloadResult" href={processedResult.processedUrl} download={processedResult.name}>Download processed audio</a>
                </section>
            )}

            {processTime !== null && (isProcessingTrack || isProcessingLiveDemo) && (
                <div className="liveMetric"><span>Current mean chunk time</span><strong>{processTime.toFixed(2)} ms</strong></div>
            )}
            {Boolean(processTimeRaw.length) && <div className="statisticsPanel"><Statistics result={processTimeRaw} /></div>}
        </div>
    );
};

export default Runner;
