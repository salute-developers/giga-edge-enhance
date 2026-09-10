# Model Bundles

## Overview

Spench denoiser models are packaged as `.bundle` resources with metadata that identifies each model. SpenchAudioKit ships with two default bundles:

- ``SpenchModelDescriptor/fast8``
- ``SpenchModelDescriptor/quality48``

Custom builds can ship additional bundles; ``SpenchModelCatalog`` discovers anything in the framework, the main bundle, or the loaded bundles list.

## Required metadata

Each model bundle must provide:

- `SpenchModelID`
- `SpenchModelFileName`
- `SpenchModelFileExtension`

## Discover available models

```swift
let models = try SpenchModelCatalog.availableModels()
```

The catalog caches discovery results for the lifetime of the process. Call
``SpenchModelCatalog/invalidateCache()`` if your application loads bundles
dynamically (e.g. through On-Demand Resources).

## Select a model

```swift
let features = SpenchFeatures(normalization: true,
                               noiseSuppression: .quality48)
```

You can also build a descriptor from a custom raw value:

```swift
let custom = SpenchModelDescriptor(rawValue: "experimental16k")
```
