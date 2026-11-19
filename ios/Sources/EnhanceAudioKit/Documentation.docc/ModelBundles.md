# Model Bundles

## Overview

GigaEdge Enhance denoiser models are packaged as `.bundle` resources with metadata that identifies each model. EnhanceAudioKit ships with three model bundles:

- ``EnhanceModelDescriptor/fast8``
- ``EnhanceModelDescriptor/quality48``
- ``EnhanceModelDescriptor/hybrid48``

Custom builds can ship additional bundles; ``EnhanceModelCatalog`` discovers anything in the framework, the main bundle, or the loaded bundles list.

## Required metadata

Each model bundle must provide:

- `EnhanceModelID`
- `EnhanceModelFileName`
- `EnhanceModelFileExtension`

## Discover available models

```swift
let models = try EnhanceModelCatalog.availableModels()
```

The catalog caches discovery results for the lifetime of the process. Call
``EnhanceModelCatalog/invalidateCache()`` if your application loads bundles
dynamically (e.g. through On-Demand Resources).

## Select a model

```swift
let features = EnhanceFeatures(normalization: true,
                               noiseSuppression: .quality48)
```

Use ``EnhanceModelDescriptor/hybrid48`` to select the hybrid 48 kHz model. Include
`EnhanceModelHybrid48.bundle` in your application's resources.

You can also build a descriptor from a custom raw value:

```swift
let custom = EnhanceModelDescriptor(rawValue: "experimental16k")
```
