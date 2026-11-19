import type { Settings } from '../../../packages/web-sdk/src/utils/common.ts';

export type DemoSettings = Partial<Settings>;
export type SettingFieldType = 'boolean' | 'number' | 'string';
export type SettingsDraft = Record<keyof Settings, string>;

type SettingDefinition = {
    key: keyof Settings;
    label: string;
    type: SettingFieldType;
    description: string;
};

export const SETTINGS_STORAGE_KEY = 'gigaedge-enhance-demo-settings';

export const DEFAULT_SETTINGS: Settings = {
    lowPerformanceWindowSeconds: 1,
    lowPerformanceMaxRefillTimes: 4,
    lowPerformanceMaxRefillTimeFactor: 3,
    minLatency: 0.064,
    maxProcessingChunkSeconds: 0.016,
    latencyIncreaseStep: 0.032,
    fetchRetries: 2,
    sharedMemoryEnabled: true,
    forceSharedMemory: false,
    dspOnAudioThread: false,
    useVadNormalization: false,
    unstable_reverseProcessingOrder: false,
    unstable_customNormalizationConfig: undefined,
    unstable_customAECConfig: undefined,
};

export const SETTINGS_DEFINITIONS: SettingDefinition[] = [
    {
        key: 'lowPerformanceWindowSeconds',
        label: 'Low performance window seconds',
        type: 'number',
        description: 'Time window used to count buffer refills before low-performance is emitted.',
    },
    {
        key: 'lowPerformanceMaxRefillTimes',
        label: 'Low performance max refill times',
        type: 'number',
        description: 'Maximum number of output buffer refills allowed before low-performance is emitted.',
    },
    {
        key: 'lowPerformanceMaxRefillTimeFactor',
        label: 'Low performance refill time factor',
        type: 'number',
        description: 'Max times output buffer size increased by Latency increase step.',
    },
    {
        key: 'minLatency',
        label: 'Min latency',
        type: 'number',
        description: 'Initial output buffer latency in seconds.',
    },
    {
        key: 'maxProcessingChunkSeconds',
        label: 'Max processing chunk',
        type: 'number',
        description:
            'Maximum WASM processing chunk size in seconds. The SDK rounds it down to 16ms steps and derives transfer chunk size automatically.',
    },
    {
        key: 'latencyIncreaseStep',
        label: 'Latency increase step',
        type: 'number',
        description: 'Latency increase in seconds applied on each step.',
    },
    {
        key: 'fetchRetries',
        label: 'Fetch retries',
        type: 'number',
        description: 'Maximum number of retry attempts for network resource loading.',
    },
    {
        key: 'sharedMemoryEnabled',
        label: 'Shared memory enabled',
        type: 'boolean',
        description: 'Allow SharedArrayBuffer transport when the environment supports it.',
    },
    {
        key: 'forceSharedMemory',
        label: 'Force shared memory',
        type: 'boolean',
        description: 'Force SharedArrayBuffer transport regardless of environment support.',
    },
    {
        key: 'dspOnAudioThread',
        label: 'DSP on audio thread',
        type: 'boolean',
        description: 'Deprecated setting that runs DSP features on the audio thread.',
    },
    {
        key: 'useVadNormalization',
        label: 'Use VAD normalization',
        type: 'boolean',
        description: 'Enable Voice Activity Detection inside normalization.',
    },
    {
        key: 'unstable_reverseProcessingOrder',
        label: 'Unstable reverse processing order',
        type: 'boolean',
        description: 'Experimental setting that reverses the processing order of ML and DSP features.',
    },
    {
        key: 'unstable_customNormalizationConfig',
        label: 'Unstable custom normalization config',
        type: 'string',
        description: 'Experimental custom normalization config in YAML-like text format.',
    },
    {
        key: 'unstable_customAECConfig',
        label: 'Unstable custom AEC config',
        type: 'string',
        description: 'Experimental custom echo cancellation config in YAML-like text format.',
    },
];

function isPlainObject(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export function formatSettingValue(value: Settings[keyof Settings] | undefined): string {
    if (typeof value === 'undefined') {
        return 'undefined';
    }

    return String(value);
}

export function getEffectiveSettings(settings: DemoSettings): Settings {
    return {
        ...DEFAULT_SETTINGS,
        ...settings,
    };
}

export function createSettingsDraft(settings: DemoSettings): SettingsDraft {
    const effectiveSettings = getEffectiveSettings(settings);

    return SETTINGS_DEFINITIONS.reduce((draft, definition) => {
        const value = effectiveSettings[definition.key];
        draft[definition.key] = typeof value === 'undefined' ? '' : String(value);
        return draft;
    }, {} as SettingsDraft);
}

export function parseSettingsDraft(draft: SettingsDraft): DemoSettings {
    return SETTINGS_DEFINITIONS.reduce((settings, definition) => {
        const rawValue = draft[definition.key];

        if (definition.type === 'boolean') {
            settings[definition.key] = (rawValue === 'true') as never;
            return settings;
        }

        if (definition.type === 'number') {
            const parsedValue = Number(rawValue);
            settings[definition.key] = (
                Number.isFinite(parsedValue) ? parsedValue : DEFAULT_SETTINGS[definition.key]
            ) as never;
            return settings;
        }

        const trimmedValue = rawValue.trim();

        if (!trimmedValue) {
            return settings;
        }

        settings[definition.key] = trimmedValue as never;
        return settings;
    }, {} as DemoSettings);
}

export function loadStoredSettings(): DemoSettings {
    const rawValue = window.localStorage.getItem(SETTINGS_STORAGE_KEY);

    if (!rawValue) {
        return {};
    }

    try {
        const parsedValue: unknown = JSON.parse(rawValue);

        if (!isPlainObject(parsedValue)) {
            return {};
        }

        return SETTINGS_DEFINITIONS.reduce((settings, definition) => {
            const value = parsedValue[definition.key];

            if (definition.type === 'boolean' && typeof value === 'boolean') {
                settings[definition.key] = value as never;
            }

            if (definition.type === 'number' && typeof value === 'number' && Number.isFinite(value)) {
                settings[definition.key] = value as never;
            }

            if (definition.type === 'string' && typeof value === 'string') {
                settings[definition.key] = value as never;
            }

            return settings;
        }, {} as DemoSettings);
    } catch {
        return {};
    }
}

export function saveStoredSettings(settings: DemoSettings) {
    window.localStorage.setItem(SETTINGS_STORAGE_KEY, JSON.stringify(settings));
}
