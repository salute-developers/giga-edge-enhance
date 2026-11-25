# ``EnhanceAudioKit``

EnhanceAudioKit is the iOS wrapper for GigaEdge Enhance audio processing.

## Overview

Use EnhanceAudioKit when you want to run GigaEdge Enhance normalization and denoising inside an `AVAudioEngine` graph or in direct Swift buffer-processing code.

- ``EnhanceAVAudioUnit`` integrates with `AVAudioEngine`.
- ``EnhanceAUAudioUnit`` exposes the underlying audio-unit implementation.
- ``EnhanceProcessor`` processes mono `Float` buffers directly.
- ``EnhanceModelCatalog`` discovers bundled denoiser models.

## Topics

### Essentials

- <doc:QuickStart>
- <doc:ThreadingAndLifecycle>
- <doc:ModelBundles>
- <doc:Telemetry>
- <doc:ErrorHandling>

### Audio Unit Integration

- ``EnhanceAVAudioUnit``
- ``EnhanceAUAudioUnit``

### Direct Buffer Processing

- ``EnhanceProcessor``

### Configuration

- ``EnhanceSettings``
- ``EnhanceFeatures``
- ``EnhanceModelDescriptor``

### Model Discovery

- ``EnhanceModelCatalog``

### EnhanceTelemetry & Performance Signals

- ``EnhanceTelemetry``
- ``EnhanceLowPerformanceEvent``
- ``NotificationUserInfoKey``

### Errors

- ``EnhanceError``
