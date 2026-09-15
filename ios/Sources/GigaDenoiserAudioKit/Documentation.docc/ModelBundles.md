# Model Bundles

## Overview

GigaDenoiser denoiser models are packaged as `.bundle` resources with metadata that identifies each model. GigaDenoiserAudioKit ships with two default bundles:

- ``ModelDescriptor/fast8``
- ``ModelDescriptor/quality48``

Custom builds can ship additional bundles; ``ModelCatalog`` discovers anything in the framework, the main bundle, or the loaded bundles list.

## Required metadata

Each model bundle must provide:

- `GigaDenoiserModelID`
- `GigaDenoiserModelFileName`
- `GigaDenoiserModelFileExtension`

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

You can also build a descriptor from a custom raw value:

```swift
let custom = ModelDescriptor(rawValue: "experimental16k")
```
