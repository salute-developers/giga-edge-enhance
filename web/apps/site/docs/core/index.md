---
title: Core SDK
---

# Core C and C++ SDK

The native SDK processes mono floating-point audio through configurable denoising, normalization,
and echo-cancellation stages. It exposes stable C and C++ public headers under
`core/include/spench/`.

Use the C++ API for native applications and the C API for ABI-oriented integrations. Existing
exported names are compatibility constraints and are not renamed for style.

## Integration

Link against an installed or packaged GigaEdge Spench build and include only the supported public
headers. Create models, configurations, and processing state before entering the real-time callback;
the callback should only pass mono floating-point buffers through an already-prepared pipeline.

Constructor and configuration types depend on the selected pipeline and inference engine. Use the
public headers and documentation from the same revision instead of including implementation classes
from `core/src/`.
