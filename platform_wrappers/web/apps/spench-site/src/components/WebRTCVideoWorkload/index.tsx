import { useCallback, useEffect, useRef, useState } from 'react';
import videoUrl from './video.mp4';
import { FrameTimeCounter } from './FrameTimeCounter';

type WorkloadMode = 'loop' | 'user';
type VideoCodec = 'vp8' | 'vp9' | 'h264';

export default function WebRTCVideoWorkload() {
    const [mode, setMode] = useState<WorkloadMode>('loop');
    const [srcWidth, setSrcWidth] = useState(1280);
    const [srcHeight, setSrcHeight] = useState(720);
    const [srcFps, setSrcFps] = useState(60);
    const [dstWidth, setDstWidth] = useState(1280);
    const [dstHeight, setDstHeight] = useState(720);
    const [codec, setCodec] = useState<VideoCodec>('vp8');
    const [bitrate, setBitrate] = useState(500);
    const [running, setRunning] = useState(false);
    const [ftUpdateInterval, setFtInterval] = useState(5000);
    const [currentFT, setCurrentFT] = useState(0);
    const [error, setError] = useState<string | null>(null);

    const ftCounterRef = useRef<FrameTimeCounter>(null);
    const ftTimerRef = useRef<ReturnType<typeof setInterval> | undefined>(undefined);
    const srcVideoRef = useRef<HTMLVideoElement>(null);
    const localVideoRef = useRef<HTMLVideoElement>(null);
    const remoteVideoRef = useRef<HTMLVideoElement>(null);
    const hiddenCanvasRef = useRef<HTMLCanvasElement>(null);
    const localPCRef = useRef<RTCPeerConnection | null>(null);
    const remotePCRef = useRef<RTCPeerConnection | null>(null);
    const srcStreamRef = useRef<MediaStream | null>(null);
    const animationFrameRef = useRef<number | null>(null);
    const mountedRef = useRef(true);

    const stopWorkload = useCallback((updateState = true) => {
        if (updateState && mountedRef.current) {
            setRunning(false);
            setCurrentFT(0);
        }

        ftCounterRef.current?.stopMonitoring();
        ftCounterRef.current?.clear();

        if (ftTimerRef.current !== undefined) {
            clearInterval(ftTimerRef.current);
            ftTimerRef.current = undefined;
        }
        if (animationFrameRef.current !== null) {
            cancelAnimationFrame(animationFrameRef.current);
            animationFrameRef.current = null;
        }

        localPCRef.current?.close();
        remotePCRef.current?.close();
        localPCRef.current = null;
        remotePCRef.current = null;

        srcStreamRef.current?.getTracks().forEach((track) => track.stop());
        srcStreamRef.current = null;

        const sourceVideo = srcVideoRef.current;
        if (sourceVideo) {
            sourceVideo.pause();
            sourceVideo.srcObject = null;
            sourceVideo.removeAttribute('src');
        }

        const localVideo = localVideoRef.current;
        if (localVideo) {
            localVideo.pause();
            localVideo.srcObject = null;
        }

        const remoteVideo = remoteVideoRef.current;
        if (remoteVideo) {
            remoteVideo.pause();
            remoteVideo.srcObject = null;
        }
    }, []);

    useEffect(() => {
        ftCounterRef.current = new FrameTimeCounter();

        return () => {
            mountedRef.current = false;
            stopWorkload(false);
        };
    }, [stopWorkload]);

    const startWorkload = async () => {
        stopWorkload();
        setError(null);

        const sourceVideo = srcVideoRef.current;
        const hiddenCanvas = hiddenCanvasRef.current;
        const localVideo = localVideoRef.current;

        if (!sourceVideo || !hiddenCanvas || !localVideo) {
            setError('Video preview elements are not available.');
            return;
        }

        try {
            ftCounterRef.current?.startMonitoring();
            ftTimerRef.current = setInterval(() => {
                const counter = ftCounterRef.current;
                if (!counter) return;
                setCurrentFT(counter.getAvgFrameTime());
                counter.clear();
            }, ftUpdateInterval);

            const localPC = new RTCPeerConnection({ iceServers: [] });
            const remotePC = new RTCPeerConnection({ iceServers: [] });
            localPCRef.current = localPC;
            remotePCRef.current = remotePC;

            let stream: MediaStream;
            if (mode === 'user') {
                stream = await navigator.mediaDevices.getUserMedia({
                    video: { width: srcWidth, height: srcHeight, frameRate: srcFps },
                });
                sourceVideo.srcObject = stream;
                sourceVideo.muted = true;
            } else {
                sourceVideo.src = videoUrl;
                sourceVideo.width = srcWidth;
                sourceVideo.height = srcHeight;
                sourceVideo.loop = true;
                sourceVideo.crossOrigin = 'anonymous';
                await sourceVideo.play();

                hiddenCanvas.width = srcWidth;
                hiddenCanvas.height = srcHeight;
                const context = hiddenCanvas.getContext('2d');
                if (!context) {
                    throw new Error('Canvas rendering is unavailable.');
                }

                const draw = () => {
                    context.drawImage(sourceVideo, 0, 0, srcWidth, srcHeight);
                    animationFrameRef.current = requestAnimationFrame(draw);
                };
                draw();
                stream = hiddenCanvas.captureStream(srcFps);
            }

            if (!mountedRef.current) {
                stream.getTracks().forEach((track) => track.stop());
                return;
            }

            srcStreamRef.current = stream;
            stream.getTracks().forEach((track) => localPC.addTrack(track, stream));

            remotePC.ontrack = (event) => {
                const remoteVideo = remoteVideoRef.current;
                const [remoteStream] = event.streams;
                if (remoteVideo && remoteStream) {
                    remoteVideo.srcObject = remoteStream;
                }
            };
            localPC.onicecandidate = (event) => remotePC.addIceCandidate(event.candidate);
            remotePC.onicecandidate = (event) => localPC.addIceCandidate(event.candidate);

            const codecs = RTCRtpReceiver.getCapabilities('video')?.codecs ?? [];
            const preferredCodecs = codecs.filter((candidate) =>
                candidate.mimeType.toLowerCase().includes(codec),
            );
            if (preferredCodecs.length > 0) {
                localPC.getTransceivers()[0]?.setCodecPreferences(preferredCodecs);
            }

            const offer = await localPC.createOffer();
            await localPC.setLocalDescription(offer);
            await remotePC.setRemoteDescription(offer);
            const answer = await remotePC.createAnswer();
            await remotePC.setLocalDescription(answer);
            await localPC.setRemoteDescription(answer);

            const sender = localPC.getSenders().find((candidate) => candidate.track?.kind === 'video');
            if (sender) {
                const parameters = sender.getParameters();
                parameters.encodings ??= [{}];
                parameters.encodings[0].maxBitrate = bitrate * 1_000;
                await sender.setParameters(parameters);
            }

            localVideo.srcObject = stream;
            await localVideo.play();

            if (mountedRef.current) {
                setRunning(true);
            }
        } catch (caughtError) {
            console.error('Unable to start WebRTC workload:', caughtError);
            stopWorkload();
            if (mountedRef.current) {
                setError(caughtError instanceof Error ? caughtError.message : 'Unable to start the video workload.');
            }
        }
    };

    const fps = currentFT > 0 ? Math.round(1000 / currentFT) : 0;

    return (
        <section className="webrtcWorkload" data-testid="webrtcWorkload">
            <header className="labToolHeader">
                <div>
                    <h3>WebRTC video workload</h3>
                    <p>Encode and decode a local video stream while measuring frame delivery time.</p>
                </div>
                <span className={running ? 'toolStatus toolStatusActive' : 'toolStatus'}>
                    {running ? 'Running' : 'Idle'}
                </span>
            </header>

            <fieldset className="workloadSettings" disabled={running}>
                <legend>Workload configuration</legend>
                <div className="workloadFormGrid">
                    <label className="workloadField workloadFieldWide">
                        <span>Source</span>
                        <select value={mode} onChange={(event) => setMode(event.target.value as WorkloadMode)}>
                            <option value="loop">Bundled loop video</option>
                            <option value="user">Webcam</option>
                        </select>
                    </label>

                    <div className="workloadGroup">
                        <h4>Source stream</h4>
                        <div className="workloadGroupGrid">
                            <label className="workloadField">
                                <span>Width</span>
                                <input min={160} type="number" value={srcWidth} onChange={(event) => setSrcWidth(Number(event.target.value))} />
                            </label>
                            <label className="workloadField">
                                <span>Height</span>
                                <input min={120} type="number" value={srcHeight} onChange={(event) => setSrcHeight(Number(event.target.value))} />
                            </label>
                            <label className="workloadField">
                                <span>Frame rate</span>
                                <div className="inputWithUnit">
                                    <input min={1} max={120} type="number" value={srcFps} onChange={(event) => setSrcFps(Number(event.target.value))} />
                                    <span>fps</span>
                                </div>
                            </label>
                        </div>
                    </div>

                    <div className="workloadGroup">
                        <h4>Decoded preview</h4>
                        <div className="workloadGroupGrid">
                            <label className="workloadField">
                                <span>Width</span>
                                <input min={160} type="number" value={dstWidth} onChange={(event) => setDstWidth(Number(event.target.value))} />
                            </label>
                            <label className="workloadField">
                                <span>Height</span>
                                <input min={120} type="number" value={dstHeight} onChange={(event) => setDstHeight(Number(event.target.value))} />
                            </label>
                        </div>
                    </div>

                    <div className="workloadGroup workloadGroupWide">
                        <h4>Encoding and metrics</h4>
                        <div className="workloadGroupGrid workloadEncodingGrid">
                            <label className="workloadField">
                                <span>Codec</span>
                                <select value={codec} onChange={(event) => setCodec(event.target.value as VideoCodec)}>
                                    <option value="vp8">VP8</option>
                                    <option value="vp9">VP9</option>
                                    <option value="h264">H.264</option>
                                </select>
                            </label>
                            <label className="workloadField">
                                <span>Maximum bitrate</span>
                                <div className="inputWithUnit">
                                    <input min={1} type="number" value={bitrate} onChange={(event) => setBitrate(Number(event.target.value))} />
                                    <span>kb/s</span>
                                </div>
                            </label>
                            <label className="workloadField">
                                <span>Metrics interval</span>
                                <div className="inputWithUnit">
                                    <input min={250} step={250} type="number" value={ftUpdateInterval} onChange={(event) => setFtInterval(Number(event.target.value))} />
                                    <span>ms</span>
                                </div>
                            </label>
                        </div>
                    </div>
                </div>
            </fieldset>

            <div className="workloadActions">
                <button
                    className={running ? 'secondaryButton' : 'primaryAction'}
                    data-testid={running ? 'stopWebRTCWorkload' : 'startWebRTCWorkload'}
                    type="button"
                    onClick={() => running ? stopWorkload() : startWorkload()}
                >
                    {running ? 'Stop workload' : 'Start workload'}
                </button>
                {running && (
                    <div className="workloadMetrics" aria-live="polite">
                        <span><small>Average frame time</small><strong>{currentFT.toFixed(2)} ms</strong></span>
                        <span><small>Delivered frame rate</small><strong>{fps} fps</strong></span>
                    </div>
                )}
            </div>

            {error && <p className="statusCard statusCardError" role="alert">{error}</p>}

            <video ref={srcVideoRef} autoPlay muted playsInline className="hiddenMedia" />
            <canvas ref={hiddenCanvasRef} className="hiddenMedia" />

            <div className={running ? 'workloadPreviews' : 'workloadPreviews workloadPreviewsHidden'}>
                <figure>
                    <figcaption>Encoded locally</figcaption>
                    <video ref={localVideoRef} width={dstWidth} height={dstHeight} autoPlay muted playsInline />
                </figure>
                <figure>
                    <figcaption>Decoded remotely</figcaption>
                    <video ref={remoteVideoRef} width={dstWidth} height={dstHeight} autoPlay playsInline />
                </figure>
            </div>
        </section>
    );
}
