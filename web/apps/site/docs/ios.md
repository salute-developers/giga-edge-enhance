---
title: iOS SDK
---

# iOS SDK

GigaEdgeSpenchAudioKit processes audio through `AVAudioEngine` or direct mono `Float` buffers. Link
the framework, embed the model bundles selected by `ModelCatalog`, and create
`DenoiserAVAudioUnit` or `Processor` before the render callback.

1. [Quick Start](/docs/source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/QuickStart/) — the first audio unit and direct buffers.
2. [Model bundles](/docs/source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/ModelBundles/) — `fast8`, `quality48`, `hybrid48`, and bundle metadata.
3. [Threading and lifecycle](/docs/source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/ThreadingAndLifecycle/) — the worker path and the render callback.
4. [Telemetry](/docs/source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/Telemetry/) — performance snapshots.
5. [Error handling](/docs/source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/ErrorHandling/) — `SpenchError` and recoverable failures.

iOS builds use the host Xcode toolchain through `tools`.
