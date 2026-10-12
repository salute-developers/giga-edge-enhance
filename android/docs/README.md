# GigaEdge Spench Android SDK documentation

Integrator-facing guides for the GigaEdge Spench Android Audio SDK. Start with the top-level
[README](../README.md) for the quickstart, then dive into the topic you need.

## Guides

- [Getting started](getting-started.md) — prerequisites, adding the SDK and model modules, and a
  minimal end-to-end example.
- [API reference](api-reference.md) — every public type and member, with parameters, units,
  defaults, exceptions, and nullability.
- [Architecture](architecture.md) — how audio flows from the Kotlin API through the JNI bridge to
  the native worker thread and ring buffers.
- [Threading](threading.md) — the concurrency contract for `process`, `setFeatures`, `reset`,
  `telemetry`, `close`, and callback delivery.
- [Telemetry and performance](telemetry-and-performance.md) — cumulative histograms, snapshot
  deltas, means, and the low-performance / adaptive-latency signal.
- [Models](models.md) — model packaging, asset layout, runtime discovery, and the missing-model
  error behavior.
- [ProGuard / R8](proguard-r8.md) — the consumer keep rules the SDK ships and why you need no manual
  rules.
- [Demo app](demo-app.md) — what the demo shows, how to run it, and the emulator microphone caveat.
- [Troubleshooting](troubleshooting.md) — common errors and how to resolve them.
