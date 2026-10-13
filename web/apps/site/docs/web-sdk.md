---
title: Web SDK
---

# Web SDK

`@gigaedge/spench-web-sdk` connects the native pipeline to Web Audio through WebAssembly, an
AudioWorklet, and a worker. Install the tested tarball from the
[distribution](distribution.md) page:

```bash
npm install ./gigaedge-spench-web-sdk-1.0.0.tgz
```

Create an `AudioContext`, then a node. `fast8` is the 8 kHz quantized model. `quality48` is the
discriminative 48 kHz model and the default above 8 kHz. `custom` is a packed `.ges` model you
supply; it is used only when you select it or when it is the only noise-suppression model
configured.

```ts
const sdk = GigaEdgeSpenchSDK.create(audioContext, {
  models: {
    noiseCancellation: { custom: customModel },
  },
});
const node = await sdk.createAudioNode({
  noiseCancellation: 'quality48',
  normalization: true,
  echoCancellation: false,
});
```

Connect `node` between a media source and `audioContext.destination`. Enable echo cancellation
only when the page can provide the reference the Web SDK expects. Shared memory needs
cross-origin isolation. Without it, the SDK uses `MessageChannel`.

The node emits `low_performance` when processing falls behind. The event reports `transport`,
`inference`, or `unknown`, plus the mean processing time. Create and close nodes with the SDK
lifetime APIs. Do not construct the worklet from the audio thread.

The [package reference](/docs/source/web/packages/web-sdk/) has the settings table, telemetry,
and the custom-model example.
