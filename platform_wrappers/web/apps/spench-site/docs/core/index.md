---
title: Core SDK
---

# Core C and C++ SDK

The native SDK processes mono floating-point audio through configurable denoising, normalization,
and echo-cancellation stages. It exposes stable C and C++ public headers under
`spench/include/spench/`.

Use the C++ API for native applications and the C API for ABI-oriented integrations. Existing
exported names are compatibility constraints and are not renamed for style.
