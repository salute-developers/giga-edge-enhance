---
title: Troubleshooting
---

# Troubleshooting

## The first output frames are silent

The native and mobile pipelines use worker-owned buffering. Initial calls can return zero-filled
output while the processing worker warms up. Keep feeding correctly sized frames and follow the
platform threading contract.

## The web demo cannot select shared memory

`SharedArrayBuffer` requires a cross-origin-isolated page. The GitHub Pages build provides this with
a same-origin COI service worker and may reload once on first visit. If isolation remains
unavailable, confirm that the page is served over HTTPS or localhost and that service workers are
enabled; the demo safely falls back to MessageChannel.

## A web model or WASM file cannot be loaded

Build the release WASM variants and run the staging script before packing or building the demo:

```bash
tools/build wasm-release
tools/build wasm-dsp-release

cd web
npm run stage:wasm:release
```

Do not manually copy individual generated files; the manifest validates that the JavaScript, WASM,
workers, worklets, types, and models belong to the same build.

## An Android emulator records a tone

Some emulator microphone modes provide a synthetic signal. Use a physical device when validating
real acoustic input. This does not indicate a GigaEdge Spench processing failure.

## A required build cannot run

Report the exact `tools` command and the concrete blocker, such as missing model access,
Docker, Xcode, a Windows toolchain, or a device. Do not substitute a weaker check silently.
