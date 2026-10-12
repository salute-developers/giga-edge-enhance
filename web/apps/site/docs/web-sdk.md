---
title: Web SDK
---

# Web SDK

The GigaEdge Spench Web SDK connects the native processing pipeline to Web Audio through WebAssembly,
AudioWorklet, and worker transports. The demo on this site uses the same packed SDK produced by the
repository build.

The static site registers a same-origin COI service worker to provide the isolated context required
for `SharedArrayBuffer` on GitHub Pages. The first visit may reload once while the service worker
takes control. Unsupported browsers continue to use MessageChannel.

For API details and complete examples, see the repository's Web SDK package documentation.

[Open the complete Web SDK reference](/docs/source/web/packages/web-sdk/)
