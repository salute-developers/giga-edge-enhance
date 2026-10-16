import { useRef, useState, useCallback } from 'react';
import './App.css';
import WebRTCVideoWorkload from './components/WebRTCVideoWorkload';
import MultipleNormTest from './components/MultipleNormTest';
import Runner, { type Features } from './components/Runner';
import SettingsModal from './components/SettingsModal';
import {
    createSettingsDraft,
    getEffectiveSettings,
    loadStoredSettings,
    parseSettingsDraft,
    saveStoredSettings,
    type DemoSettings,
    type SettingsDraft,
} from './settings';

function getInitialSettings(): DemoSettings {
    return loadStoredSettings();
}

function getTransportFromSettings(settings: DemoSettings): 'message' | 'shared' {
    return getEffectiveSettings(settings).sharedMemoryEnabled && window.crossOriginIsolated ? 'shared' : 'message';
}

function App() {
    const initialSettings = getInitialSettings();
    const setFeaturesRef = useRef<(f: Features) => Promise<void | Error>>(null);
    const customDenoiserInputRef = useRef<HTMLInputElement>(null);
    const [features, setFeatures] = useState<Features>({
        noiseCancellation: 'quality48',
        normalization: true,
        echoCancellation: false,
    });
    const [sampleRate, setSampleRate] = useState<number>(48000);
    const [loadingFeature, setLoadingFeature] = useState(false);
    const [customDenoiseModel, setCustomDenoiseModel] = useState<ArrayBuffer | undefined>();
    const [settings, setSettings] = useState<DemoSettings>(initialSettings);
    const [settingsDraft, setSettingsDraft] = useState<SettingsDraft>(() => createSettingsDraft(initialSettings));
    const [isSettingsOpen, setIsSettingsOpen] = useState(false);
    const [isProcessing, setIsProcessing] = useState(false);

    const applySettingsState = useCallback((nextSettings: DemoSettings) => {
        setSettings(nextSettings);
        saveStoredSettings(nextSettings);
    }, []);

    const transport = getTransportFromSettings(settings);
    const vadNorm = Boolean(settings.useVadNormalization);

    const openSettingsModal = useCallback(() => {
        setSettingsDraft(createSettingsDraft(settings));
        setIsSettingsOpen(true);
    }, [settings]);

    const closeSettingsModal = useCallback(() => {
        setIsSettingsOpen(false);
    }, []);

    const handleSettingsDraftChange = useCallback(<T extends keyof SettingsDraft>(key: T, value: SettingsDraft[T]) => {
        setSettingsDraft((currentDraft) => ({
            ...currentDraft,
            [key]: value,
        }));
    }, []);

    const handleApplySettings = useCallback(() => {
        const nextSettings = parseSettingsDraft(settingsDraft);

        applySettingsState(nextSettings);
        setIsSettingsOpen(false);
    }, [applySettingsState, settingsDraft]);

    const handleTransportChange = useCallback(
        (nextTransport: 'message' | 'shared') => {
            applySettingsState({
                ...settings,
                sharedMemoryEnabled: nextTransport === 'shared',
            });
        },
        [applySettingsState, settings],
    );

    const handleVadNormChange = useCallback(() => {
        const nextVadNorm = !vadNorm;

        applySettingsState({
            ...settings,
            useVadNormalization: nextVadNorm,
        });
    }, [applySettingsState, settings, vadNorm]);

    const handleFeatureChange = useCallback(
        async <T extends keyof Features>(feature: T, value: Features[T]) => {
            const newFeatures = { ...features, [feature]: value };

            console.log('setFeatures', newFeatures);
            setFeatures(newFeatures);
            if (setFeaturesRef.current) {
                setLoadingFeature(true);
                try {
                    await setFeaturesRef.current(newFeatures);
                } catch {
                    // pass
                } finally {
                    setLoadingFeature(false);
                }
            }
        },
        [features],
    );

    const handleDenoiserChange = useCallback(
        (noiseCancellation: Features['noiseCancellation'], requiredSampleRate?: number) => {
            if (requiredSampleRate) {
                setSampleRate(requiredSampleRate);
            }
            return handleFeatureChange('noiseCancellation', noiseCancellation);
        },
        [handleFeatureChange],
    );

    const handleCustomDenoiserModel = useCallback(async () => {
        if (!customDenoiserInputRef.current || !customDenoiserInputRef.current.files) {
            return;
        }

        const [file] = customDenoiserInputRef.current.files;

        const reader = new FileReader();
        reader.onload = function () {
            const arrayBuffer = reader.result;
            if (arrayBuffer instanceof ArrayBuffer) {
                setCustomDenoiseModel(arrayBuffer);
                handleFeatureChange('noiseCancellation', 'custom');
            } else {
                console.error('reader.result is not arraybuffer');
            }
        };
        reader.onerror = function (e) {
            console.error('Error reading file:', e);
        };

        reader.readAsArrayBuffer(file);
    }, [handleFeatureChange]);

    return (
        <main className="demoShell">
            <header className="demoHeader">
                <div>
                    <p className="eyebrow">Web SDK demo</p>
                    <h1>Audio processing workbench</h1>
                </div>
                <button className="secondaryButton" type="button" onClick={openSettingsModal}>
                    Advanced settings
                </button>
            </header>

            <div className="runtimeBar" aria-label="Browser audio runtime">
                <span className="runtimeItem"><span className="statusDot" /> Audio ready</span>
                <span className="runtimeItem">Transport <strong>{transport === 'shared' ? 'Shared memory' : 'Message channel'}</strong></span>
                <span className="runtimeItem">Sample rate <strong>{sampleRate / 1000} kHz</strong></span>
                <span className="runtimeItem">Isolation <strong>{window.crossOriginIsolated ? 'Available' : 'Unavailable'}</strong></span>
            </div>

            <div className="workbenchGrid">
                <section className="panel controlPanel" aria-labelledby="processing-setup-title">
                    <div className="panelHeader">
                        <div>
                            <p className="panelKicker">Processing setup</p>
                            <h2 id="processing-setup-title">Shape the signal</h2>
                        </div>
                        {loadingFeature && <span data-testid="loadingFeature" className="loadingBadge">Updating…</span>}
                    </div>

                    <div className="controlGroup">
                        <div className="controlHeading">
                            <h3>Noise suppression</h3>
                            <p>Choose quality, speed, or bring your own packed model.</p>
                        </div>
                        <div className="choiceGrid">
                            <label className="choiceCard">
                                <input type="radio" data-testid="denoiserNone" checked={features.noiseCancellation === false} name="denoiserChoice" onChange={() => handleFeatureChange('noiseCancellation', false)} />
                                <span><strong>Off</strong><small>Pass the signal through</small></span>
                            </label>
                            <label className="choiceCard">
                                <input type="radio" data-testid="denoiserQuality48" checked={features.noiseCancellation === 'quality48'} name="denoiserChoice" onChange={() => handleDenoiserChange('quality48', 48000)} />
                                <span><strong>Quality</strong><small>Best voice quality · 48 kHz</small></span>
                            </label>
                            <label className="choiceCard">
                                <input type="radio" data-testid="denoiserHybrid48" checked={features.noiseCancellation === 'hybrid48'} name="denoiserChoice" onChange={() => handleDenoiserChange('hybrid48', 48000)} />
                                <span><strong>Hybrid</strong><small>Hybrid enhancement · 48 kHz</small></span>
                            </label>
                            <label className="choiceCard">
                                <input type="radio" data-testid="denoiserFast8" checked={features.noiseCancellation === 'fast8'} name="denoiserChoice" onChange={() => handleDenoiserChange('fast8', 8000)} />
                                <span><strong>Fast</strong><small>Lower CPU use · 8 kHz</small></span>
                            </label>
                            <label className="choiceCard customChoice">
                                <input type="radio" data-testid="denoiserCustom" checked={features.noiseCancellation === 'custom'} name="denoiserChoice" onChange={() => customDenoiseModel ? handleFeatureChange('noiseCancellation', 'custom') : handleFeatureChange('noiseCancellation', false)} />
                                <span><strong>Custom</strong><small>{customDenoiseModel ? 'Model ready' : 'Upload a .ges model'}</small></span>
                                <input className="modelInput" aria-label="Upload custom denoiser model" data-testid="customDenoiserModelInput" id="customDenoiserModelInput" type="file" ref={customDenoiserInputRef} accept=".ges,application/octet-stream" multiple={false} onChange={handleCustomDenoiserModel} />
                            </label>
                        </div>
                    </div>

                    <div className="switchList">
                        <label className="switchRow">
                            <span><strong>Loudness normalization</strong><small>Keep speech level consistent</small></span>
                            <input checked={Boolean(features.normalization)} data-testid="normalizationToggle" onChange={() => handleFeatureChange('normalization', !features.normalization)} type="checkbox" role="switch" />
                        </label>
                        {features.normalization && (
                            <label className="switchRow nestedSwitch">
                                <span><strong>Voice activity detection</strong><small>Normalize active speech only</small></span>
                                <input checked={vadNorm} data-testid="vadNormalizationToggle" onChange={handleVadNormChange} type="checkbox" role="switch" />
                            </label>
                        )}
                        <label className="switchRow">
                            <span><strong>Echo cancellation</strong><small>Remove the far-end reference signal</small></span>
                            <input checked={Boolean(features.echoCancellation)} data-testid="aecToggle" onChange={() => handleFeatureChange('echoCancellation', !features.echoCancellation)} type="checkbox" role="switch" />
                        </label>
                    </div>

                    <div className="runtimeControls">
                        <label>
                            <span>Transport</span>
                            <select data-testid="transportSelect" disabled={!window.crossOriginIsolated} value={transport} onChange={(e) => handleTransportChange(e.target.value as 'message' | 'shared')}>
                                <option data-testid="transportMessage" value="message">MessageChannel</option>
                                <option data-testid="transportShared" value="shared">SharedArrayBuffer</option>
                            </select>
                        </label>
                        <label>
                            <span>Sample rate</span>
                            <div className="inputWithUnit">
                                <input value={sampleRate} data-testid="sampleRateInput" onChange={(e) => setSampleRate(Number(e.target.value))} min={8000} max={96000} step={1000} type="number" />
                                <span>Hz</span>
                            </div>
                        </label>
                    </div>
                    {!window.crossOriginIsolated && <p className="compatibilityNote">Shared memory needs COOP/COEP response headers. GitHub Pages uses MessageChannel automatically.</p>}
                </section>

                <section className="panel runnerPanel" aria-label="Process and inspect audio">
                    <Runner customDenoiseModel={customDenoiseModel} features={features} sampleRate={sampleRate} setFeaturesRef={setFeaturesRef} settings={settings} onProcessingChange={setIsProcessing} />
                </section>
            </div>

            <section className="performanceSection">
                <div className="sectionIntro">
                    <p className="panelKicker">Engineering tools</p>
                    <h2>Performance lab</h2>
                    <p>Stress the audio graph or add a realistic WebRTC video workload while profiling.</p>
                </div>
                <details className="labDisclosure">
                    <summary>Audio node stress test</summary>
                    <div className="labContent"><MultipleNormTest /></div>
                </details>
                <details className="labDisclosure">
                    <summary>WebRTC video workload</summary>
                    <div className="labContent"><WebRTCVideoWorkload /></div>
                </details>
            </section>

            <SettingsModal
                isOpen={isSettingsOpen}
                isProcessing={isProcessing}
                settingsDraft={settingsDraft}
                onClose={closeSettingsModal}
                onApply={handleApplySettings}
                onChange={handleSettingsDraftChange}
            />
        </main>
    );
}

export default App;
