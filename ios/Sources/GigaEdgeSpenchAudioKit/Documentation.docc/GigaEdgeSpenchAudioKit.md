# ``GigaEdgeSpenchAudioKit``

GigaEdgeSpenchAudioKit is the iOS wrapper for GigaEdge Spench audio processing.

## Overview

Use GigaEdgeSpenchAudioKit when you want to run GigaEdge Spench normalization and denoising inside an `AVAudioEngine` graph or in direct Swift buffer-processing code.

- ``DenoiserAVAudioUnit`` integrates with `AVAudioEngine`.
- ``DenoiserAUAudioUnit`` exposes the underlying audio-unit implementation.
- ``Processor`` processes mono `Float` buffers directly.
- ``ModelCatalog`` discovers bundled denoiser models.

## Topics

### Essentials

- <doc:QuickStart>
- <doc:ThreadingAndLifecycle>
- <doc:ModelBundles>
- <doc:Telemetry>
- <doc:ErrorHandling>

### Audio Unit Integration

- ``DenoiserAVAudioUnit``
- ``DenoiserAUAudioUnit``

### Direct Buffer Processing

- ``Processor``

### Configuration

- ``Settings``
- ``Features``
- ``ModelDescriptor``

### Model Discovery

- ``ModelCatalog``

### Telemetry & Performance Signals

- ``Telemetry``
- ``LowPerformanceEvent``
- ``NotificationUserInfoKey``

### Errors

- ``DenoiserError``
