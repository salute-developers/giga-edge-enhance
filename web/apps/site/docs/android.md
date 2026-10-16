---
title: Android SDK
---

# Android SDK

The Android SDK processes mono `FloatArray` PCM on a background worker. Applications add the SDK
module and the model modules they ship, then call `Processor.process`.

1. [Getting started](/docs/source/android/docs/getting-started/) — Gradle setup and the first call.
2. [Models](/docs/source/android/docs/models/) — the `quality48` and `hybrid48` packages and runtime discovery.
3. [Threading](/docs/source/android/docs/threading/) — `process`, `setFeatures`, `reset`, and `close`.
4. [Telemetry and performance](/docs/source/android/docs/telemetry-and-performance/) — timing histograms and low-performance signals.
5. [Troubleshooting](/docs/source/android/docs/troubleshooting/) — startup and model errors.

The [API reference](/docs/source/android/docs/api-reference/) lists the Kotlin types. Architecture,
ProGuard/R8, and the demo app are in the same section of the sidebar.
