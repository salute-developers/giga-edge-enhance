# ``SpenchAudioKit``

SpenchAudioKit is the iOS wrapper for GigaEdge Spench audio processing.

## Overview

Use SpenchAudioKit when you want to run Spench normalization and denoising inside an `AVAudioEngine` graph or in direct Swift buffer-processing code.

- ``SpenchAVAudioUnit`` integrates with `AVAudioEngine`.
- ``SpenchAUAudioUnit`` exposes the underlying audio-unit implementation.
- ``SpenchProcessor`` processes mono `Float` buffers directly.
- ``SpenchModelCatalog`` discovers bundled denoiser models.

## Topics

### Essentials

- <doc:QuickStart>
- <doc:ThreadingAndLifecycle>
- <doc:ModelBundles>
- <doc:Telemetry>
- <doc:ErrorHandling>

### Audio Unit Integration

- ``SpenchAVAudioUnit``
- ``SpenchAUAudioUnit``

### Direct Buffer Processing

- ``SpenchProcessor``

### Configuration

- ``SpenchSettings``
- ``SpenchFeatures``
- ``SpenchModelDescriptor``

### Model Discovery

- ``SpenchModelCatalog``

### Telemetry & Performance Signals

- ``SpenchTelemetry``
- ``SpenchLowPerformanceEvent``
- ``SpenchNotificationUserInfoKey``

### Errors

- ``SpenchError``
