# Model Bundles

## Overview

GigaEdge Spench denoiser models are packaged as `.bundle` resources with metadata that identifies each model. GigaEdgeSpenchAudioKit ships with three model bundles:

- ``ModelDescriptor/fast8``
- ``ModelDescriptor/quality48``
- ``ModelDescriptor/hybrid48``

Custom builds can ship additional bundles; ``ModelCatalog`` discovers anything in the framework, the main bundle, or the loaded bundles list.

## Required metadata

Each model bundle must provide:

- `GigaEdgeSpenchModelID`
- `GigaEdgeSpenchModelFileName`
- `GigaEdgeSpenchModelFileExtension`

## Discover available models

```swift
let models = try ModelCatalog.availableModels()
```

The catalog caches discovery results for the lifetime of the process. Call
``ModelCatalog/invalidateCache()`` if your application loads bundles
dynamically (e.g. through On-Demand Resources).

## Select a model

```swift
let features = Features(normalization: true,
                               noiseSuppression: .quality48)
```

Use ``ModelDescriptor/hybrid48`` to select the hybrid 48 kHz model. Include
`GigaEdgeSpenchModelHybrid48.bundle` in your application's resources.

You can also build a descriptor from a custom raw value:

```swift
let custom = ModelDescriptor(rawValue: "experimental16k")
```
