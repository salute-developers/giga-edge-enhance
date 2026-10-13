# GigaEdge Spench Web SDK

**GigaEdge Spench** is a real‑time audio‑processing library that runs in the browser using
Web Audio, Web Workers and, optionally, a WebAssembly‑backed DSP worklet.
The SDK exposes a single class – `GigaEdgeSpenchSDK` – that lets you create a
`GigaEdgeSpenchAudioNode` and enable any combination of the following features:

| Feature | Description |
|---------|-------------|
| `normalization` | Sound normalization. |
| `echoCancellation` | Acoustic echo cancellation. |
| `noiseCancellation` | Acoustic noise cancellation. |

The SDK also exposes a tiny telemetry API for measuring performance, a pluggable
logger and an optional “shared‑memory” transport that reduces message‑passing
overhead.

> ⚡ **Tip** – The workbench in `apps/site` shows how to wire the
> node into a media graph, measure performance and even run a live demo using the
> browser microphone.

## Installation

The tested package is the npm tarball inside the
`gigaedge-spench-web-sdk-<tag>.zip` GitHub Release asset. Install that file
directly:

```bash
npm install ./gigaedge-spench-web-sdk-1.0.0.tgz
```

The package name is `@gigaedge/spench-web-sdk`. The release workflow does not
publish it to the npm registry; the registry and GitHub Releases can both carry
this same tarball when registry publication is enabled. See
[Distribution](/docs/distribution/).

The SDK uses native ES modules. Bundle it with Rollup, Vite, or another
transpiler when targeting older browsers.

## Quickstart

```ts
import { GigaEdgeSpenchSDK, type Features } from '@gigaedge/spench-web-sdk';

// 1️⃣ Create an AudioContext
const audioCtx = new AudioContext();

// 2️⃣ Create an SDK instance
const sdk = GigaEdgeSpenchSDK.create(audioCtx);

// 3️⃣ Define the feature set
const features: Features = {
  noiseCancellation: 'quality48',
  normalization: true,
  echoCancellation: false,
};

// 4️⃣ Create a node
const gigaEdgeSpenchNode = await sdk.createAudioNode(features);

// 5️⃣ Hook it into a graph
const source = audioCtx.createMediaStreamSource(stream);
source.connect(gigaEdgeSpenchNode);
gigaEdgeSpenchNode.connect(audioCtx.destination);
```

## API Reference

### `GigaEdgeSpenchSDK`

| Method | Description |
|--------|-------------|
| `static isEnvironmentSupported(): boolean` | Quick check if the browser has the required Web Audio APIs. |
| `static create(ctx, params?): GigaEdgeSpenchSDK` | Factory – returns a new SDK instance. |
| `createAudioNode(features?, settings?): Promise<GigaEdgeSpenchAudioNode \| LoadError \| SetFeaturesFailedError>` | Create and configure a node. |

> The SDK internally preloads the worklet module, the optional DSP WASM and the
> requested model files.  The returned node already emits a `destroyed` event
> when the underlying worker or worklet terminates.

### `GigaEdgeSpenchSDK.create(ctx: AudioContext, params?: CreateGigaEdgeSpenchParams<M>)`

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `ctx` | `AudioContext` | The Web Audio context that will own the node. | – |
| `params.loggerVerbosity` | `Verbosity` (`'verbose' \| 'normal' \| 'silent'`) | Sets the internal logger level.  `'silent'` disables all console output. | `'normal'` |
| `params.logger` | `ExternalLogger` | Optional external logger.  If provided, it will be used for all SDK‑generated log messages. | `undefined` |
| `params.sendMetric` | `SendMetricFunction` | Optional callback for telemetry metrics.  If omitted the SDK will silently drop all metrics. | `undefined` |
| `params.preloadFeatures` | `Partial<Features<M>>` | Feature set to preload (download WASM, worker and model files) before the first node is created. | `undefined` |
| `params.models` | `Partial<M>` | Custom model URLs or ArrayBuffers that will override the default model map (`DEFAULT_MODELS`).  Useful for loading locally hosted or user‑provided models. | `undefined` |
| `params.settings` | `Partial<Settings>` | User‑supplied overrides for the SDK configuration.  All other settings use the built‑in defaults defined in `utils/common.ts`. | `undefined` |

### `GigaEdgeSpenchSDK.createAudioNode(features?: Partial<Features<M>>, settings?: Partial<Settings>)`

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

### `GigaEdgeSpenchAudioNode`

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

The SDK bundles `fast8.ges` as `fast8` and `quality48.ges` as `quality48`. Supply other model keys through
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

`cd apps/site && npm start`

```bash
# From the repo root
cd apps/site
npm start & npm run pw:open
```

## Demo App

The folder `apps/site` contains a React workbench that demonstrates
all SDK capabilities:

1. **Feature toggles** – noise cancellation, normalization and echo cancellation.
2. **Transport selection** – MessageChannel vs SharedArrayBuffer.
3. **Live demo** – connects to the microphone, processes the stream and
   records both raw & processed audio.
4. **Benchmark** – runs a pre‑packaged audio file, records frame times and
   shows statistics.
5. **Multiple normalizer nodes test** – stress‑tests the SDK by creating
   and destroying 10 000 nodes.

> **Running the workbench** – `cd apps/site && npm start`

## Example – Custom Models

If you have a custom model you can load it directly:

```ts
const customModel = await fetch('/path/to/my/denoise.ges').arrayBuffer();

const sdk = GigaEdgeSpenchSDK.create(audioCtx, {
  models: {
    noiseCancellation: { quality48: customModel },
  },
});
```

The node will use the supplied ArrayBuffer instead of downloading the default model.

## Example — In Details

```ts
import { GigaEdgeSpenchAudioNode, GigaEdgeSpenchError, GigaEdgeSpenchSDK } from '@gigaedge/spench-web-sdk';

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
const sdk = GigaEdgeSpenchSDK.create(ctx, {
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
const gigaEdgeSpenchNode: GigaEdgeSpenchAudioNode | GigaEdgeSpenchError = await sdk.createAudioNode();
if (gigaEdgeSpenchNode instanceof GigaEdgeSpenchError) {
  throw gigaEdgeSpenchNode;
}

// Triggered when the system is under heavy load; disable effects to prevent audio artifacts.
gigaEdgeSpenchNode.addEventListener('low_performance', () => {});

// Triggered when an unexpected worker error occurs; disable the node.
gigaEdgeSpenchNode.addEventListener('destroyed', () => {});

const callStream: MediaStream; // Input stream
const sourceStreamNode = ctx.createMediaStreamSource(callStream);
const destinationStreamNode = ctx.createMediaStreamDestination();
sourceStreamNode.connect(gigaEdgeSpenchNode).connect(destinationStreamNode);

// The resulting stream with noise cancelled.
const outputStream = destinationStreamNode.stream;

const res = await gigaEdgeSpenchNode.setFeatures(features);
if (res instanceof GigaEdgeSpenchError) {
  throw res;
}

// The node can be reused across calls; invoke reset() after each call completes.
gigaEdgeSpenchNode.reset();

// Release resources when finished.
gigaEdgeSpenchNode.destroy();
await ctx.close();
```

## FAQ

| Question | Answer |
|----------|--------|
| **Can I run DSP on the audio thread?** | Yes – set `settings.dspOnAudioThread = true` in the SDK creation.  The worklet will load the DSP‑WASM and run the DSP pipeline on the audio thread. |
| **What if the browser does not support SharedArrayBuffer?** | The SDK automatically falls back to `MessageChannel`.  You can also force it via `settings.forceSharedMemory`. |
| **Does the SDK clean up after me?** | Call `node.destroy()` when you are done.  The node also listens to the AudioContext state and destroys itself when the context is closed. |
