---
title: Configuration
---

# Pipeline configuration

GigaEdge Spench supports programmatic feature selection and YAML configuration. A typical real-time pipeline
combines one denoiser model with optional normalization and acoustic echo cancellation.

Model sample rate, input sample rate, channel layout, and reference-signal requirements must match
the selected processing path. Consult the platform API reference before changing latency or buffer
settings on an audio thread.
