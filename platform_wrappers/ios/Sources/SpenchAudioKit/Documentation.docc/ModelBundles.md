# Model Bundles

## Overview

Spench denoiser models are packaged as `.bundle` resources with metadata that identifies each model. SpenchAudioKit ships with three default bundles:

- ``SpenchModelDescriptor/lite8k``
- ``SpenchModelDescriptor/lite24k``
- ``SpenchModelDescriptor/full24k``

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
                              noiseSuppression: .lite24k)
```

You can also build a descriptor from a custom raw value:

```swift
let custom = SpenchModelDescriptor(rawValue: "experimental16k")
```
