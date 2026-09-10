# Spench Web SDK

**Spench** is a real‑time audio‑processing library that runs in the browser using
Web Audio, Web Workers and, optionally, a WebAssembly‑backed DSP worklet.
The SDK exposes a single class – `SpenchSDK` – that lets you create a
`SpenchAudioNode` and enable any combination of the following features:

| Feature | Description |
|---------|-------------|
| `normalization` | Sound normalization. |
| `echoCancellation` | Acoustic echo cancellation. |
| `noiseCancellation` | Acoustic noise cancellation. |

The SDK also exposes a tiny telemetry API for measuring performance, a pluggable
logger and an optional “shared‑memory” transport that reduces message‑passing
overhead.

> ⚡ **Tip** – The workbench in `apps/spench-site` shows how to wire the
> node into a media graph, measure performance and even run a live demo using the
> browser microphone.

## Installation

```bash
# Yarn
yarn add @gigaedge/spench-web-sdk

# NPM
npm i @gigaedge/spench-web-sdk
```

> The SDK uses native ES modules. If you target older browsers, bundle with
> a tool that transpiles them (e.g. Rollup, Vite).

## Quickstart

```ts
import { SpenchSDK, type Features } from '@gigaedge/spench-web-sdk';

// 1️⃣ Create an AudioContext
const audioCtx = new AudioContext();

// 2️⃣ Create an SDK instance
const sdk = SpenchSDK.create(audioCtx);

// 3️⃣ Define the feature set
const features: Features = {
  noiseCancellation: 'quality48',
  normalization: true,
  echoCancellation: false,
};

// 4️⃣ Create a node
const spenchNode = await sdk.createAudioNode(features);

// 5️⃣ Hook it into a graph
const source = audioCtx.createMediaStreamSource(stream);
source.connect(spenchNode);
spenchNode.connect(audioCtx.destination);
```

## API Reference

### `SpenchSDK`

| Method | Description |
|--------|-------------|
| `static isEnvironmentSupported(): boolean` | Quick check if the browser has the required Web Audio APIs. |
| `static create(ctx, params?): SpenchSDK` | Factory – returns a new SDK instance. |
| `createAudioNode(features?, settings?): Promise<SpenchAudioNode \| LoadError \| SetFeaturesFailedError>` | Create and configure a node. |

> The SDK internally preloads the worklet module, the optional DSP WASM and the
> requested model files.  The returned node already emits a `destroyed` event
> when the underlying worker or worklet terminates.

### `SpenchSDK.create(ctx: AudioContext, params?: CreateSpenchParams<M>)`

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `ctx` | `AudioContext` | The Web Audio context that will own the node. | – |
| `params.loggerVerbosity` | `Verbosity` (`'verbose' \| 'normal' \| 'silent'`) | Sets the internal logger level.  `'silent'` disables all console output. | `'normal'` |
| `params.logger` | `ExternalLogger` | Optional external logger.  If provided, it will be used for all SDK‑generated log messages. | `undefined` |
| `params.sendMetric` | `SendMetricFunction` | Optional callback for telemetry metrics.  If omitted the SDK will silently drop all metrics. | `undefined` |
| `params.preloadFeatures` | `Partial<Features<M>>` | Feature set to preload (download WASM, worker and model files) before the first node is created. | `undefined` |
| `params.models` | `Partial<M>` | Custom model URLs or ArrayBuffers that will override the default model map (`DEFAULT_MODELS`).  Useful for loading locally hosted or user‑provided models. | `undefined` |
| `params.settings` | `Partial<Settings>` | User‑supplied overrides for the SDK configuration.  All other settings use the built‑in defaults defined in `utils/common.ts`. | `undefined` |

### `SpenchSDK.createAudioNode(features?: Partial<Features<M>>, settings?: Partial<Settings>)`

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `features` | `Partial<Features<M>>` | The initial feature set for the node.  If omitted, the node starts in a *neutral* state (no DSP or main features). | `undefined` |
| `settings` | `Partial<Settings>` | Settings that will merge with the SDK defaults **only for this node**.  They override the global SDK settings passed to `create`. | `undefined` |

> **Important** – `createAudioNode` **preloads** the requested features before returning the node.  The promise resolves with the node, or an error (`LoadError` or `SetFeaturesFailedError`) if the preload or configuration fails.

### Settings

The `Settings` object contains all configurable knobs.  Its properties are
exposed in `src/utils/common.ts` and have the following defaults:

| Property | Description | Default |
|----------|-------------|---------|
| `lowPerformanceWindowSeconds` | Time window for counting buffer refills. | `1` |
| `lowPerformanceMaxRefillTimes` | Max refills before emitting `low_performance`. | `4` |
| `lowPerformanceMaxRefillTimeFactor` | Max times output buffer size increased by latencyIncreaseStep. | `3` |
| `minLatency` | Initial buffer latency (seconds). | `0.064` |
| `latencyIncreaseStep` | Latency increase per refill step (seconds). | `0.032` |
| `fetchRetries` | Number of HTTP retry attempts. | `2` |
| `sharedMemoryEnabled` | Whether to try SharedArrayBuffer (if supported). | `true` |
| `forceSharedMemory` | Force SharedArrayBuffer regardless of support. | `false` |
| `dspOnAudioThread` | *Deprecated* – run DSP on audio thread. | `false` |
| `useVadNormalization` | Use Voice Activity Detection feature for audio normalization. | `false` |
| `unstable_reverseProcessingOrder` | Reverse the processing order of ML and DSP features. | `false` |
| `unstable_customNormalizationConfig` | Custom config string for normalization. | `undefined` |
| `unstable_customAECConfig` | Custom config string for echo‑cancellation. | `undefined` |

All settings can be overridden per‑node via the `settings` argument to
`createAudioNode`.  Settings that are not overridden simply inherit the SDK
defaults.

### `SpenchAudioNode`

This is a subclass of the native `AudioWorkletNode` and has the following
additions:

| Method | Description |
|--------|-------------|
| `setFeatures(features): Promise<void \| LoadError \| SetFeaturesFailedError>` | Re‑configure the node on the fly. |
| `reset(): void` | Reset DSP state. |
| `destroy(): void` | Clean up all resources. |
| `addEventListener('low_performance', ...)` | Listen for low‑performance events. |
| `addEventListener('destroyed', ...)` | Listen for node destruction. |

**Low‑performance** – the node emits `low_performance` when it detects that
audio processing is lagging.  The event contains the most probable cause
(`transport`, `inference`, or `unknown`) and the mean processing time.

### `Features`

| Property | Type | Default |
|----------|------|---------|
| `normalization` | `boolean` | `false` |
| `echoCancellation` | `boolean` | `false` |
| `noiseCancellation` | `boolean \| "fast8" \| "quality48"` | `false` |

The SDK bundles `fast8.spench` as `fast8` and `quality48.spench` as `quality48`. Supply other model keys through
`params.models` when using custom model files.

## Testing

Run unit test with:

```bash
# From the repo root
npm test
# or
yarn test
```

The tests use Jest and run in both the browser and Node
(for the worker / shared memory logic).

Run integration tests with:

`cd apps/spench-site && npm start`

```bash
# From the repo root
cd apps/spench-site
npm start & npm run pw:open
```

## Demo App

The folder `apps/spench-site` contains a React workbench that demonstrates
all SDK capabilities:

1. **Feature toggles** – noise cancellation, normalization and echo cancellation.
2. **Transport selection** – MessageChannel vs SharedArrayBuffer.
3. **Live demo** – connects to the microphone, processes the stream and
   records both raw & processed audio.
4. **Benchmark** – runs a pre‑packaged audio file, records frame times and
   shows statistics.
5. **Multiple normalizer nodes test** – stress‑tests the SDK by creating
   and destroying 10 000 nodes.

> **Running the workbench** – `cd apps/spench-site && npm start`

## Example – Custom Models

If you have a custom model you can load it directly:

```ts
const customModel = await fetch('/path/to/my/denoise.spench').arrayBuffer();

const sdk = SpenchSDK.create(audioCtx, {
  models: {
    noiseCancellation: { lite48k: customModel },
  },
});
```

The node will use the supplied ArrayBuffer instead of downloading the default model.

## Example — In Details

```ts
import { SpenchAudioNode, SpenchError, SpenchSDK } from '@gigaedge/spench-web-sdk';

// Define the sample rate (SR) for the incoming stream, or leave undefined to use the device's default SR.
const sampleRate: number | undefined = 48000;

// The AudioContext may start in a suspended state. Call await ctx.resume() to activate it.
// Some browsers require a user gesture before audio playback starts. See the MDN page for details.
// https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/state
const ctx = new AudioContext({ sampleRate });

// Model accuracy depends on input signal normalization; enable it below.
const features = {
  noiseCancellation: 'quality48' as const, // Use 'fast8' for 8 kHz input.
  normalization: true,
} as const;

// Create the SDK instance.
// Set to 'verbose' for detailed debugging output.
const sdk = SpenchSDK.create(ctx, {
  loggerVerbosity: 'verbose',
  // Provide an external logger implementation if desired.
  logger: { info: ..., debug: ..., ... },
  // Preloads features to reduce initialization time.
  preloadFeatures: features,
  // Record metrics; safe to ignore during pilot deployments.
  sendMetric: (metric, value) => console.log(metric, value),
});

// Features can be set during creation or later with setFeatures.
// Setting features after creation integrates the node into the graph more quickly,
// even if noise‑cancellation resources aren't yet available.
const spenchNode: SpenchAudioNode | SpenchError = await sdk.createAudioNode();
if (spenchNode instanceof SpenchError) {
  throw spenchNode;
}

// Triggered when the system is under heavy load; disable effects to prevent audio artifacts.
spenchNode.addEventListener('low_performance', () => {});

// Triggered when an unexpected worker error occurs; disable the node.
spenchNode.addEventListener('destroyed', () => {});

const callStream: MediaStream; // Input stream
const sourceStreamNode = ctx.createMediaStreamSource(callStream);
const destinationStreamNode = ctx.createMediaStreamDestination();
sourceStreamNode.connect(spenchNode).connect(destinationStreamNode);

// The resulting stream with noise cancelled.
const outputStream = destinationStreamNode.stream;

const res = await spenchNode.setFeatures(features);
if (res instanceof SpenchError) {
  throw res;
}

// The node can be reused across calls; invoke reset() after each call completes.
spenchNode.reset();

// Release resources when finished.
spenchNode.destroy();
await ctx.close();
```

## FAQ

| Question | Answer |
|----------|--------|
| **Can I run DSP on the audio thread?** | Yes – set `settings.dspOnAudioThread = true` in the SDK creation.  The worklet will load the DSP‑WASM and run the DSP pipeline on the audio thread. |
| **What if the browser does not support SharedArrayBuffer?** | The SDK automatically falls back to `MessageChannel`.  You can also force it via `settings.forceSharedMemory`. |
| **Does the SDK clean up after me?** | Call `node.destroy()` when you are done.  The node also listens to the AudioContext state and destroys itself when the context is closed. |
