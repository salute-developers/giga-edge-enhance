# Threading and Lifecycle

## Overview

`SpenchAUAudioUnit` keeps the render callback lightweight by moving DSP
work onto a dedicated worker thread. The render callback only moves
samples between an SPSC ring buffer and the host's `AudioBufferList`,
and the worker reads from that ring, runs the Spench pipeline, and
writes processed samples back to a second ring.

```
[render thread] ── push ─▶ inputRing ── pop ─▶ [worker thread] ── push ─▶ outputRing ── pop ─▶ [render thread]
```

## Render-thread guarantees

The render block:

- Never allocates.
- Never takes locks.
- Never crosses the Objective-C runtime.
- Returns silence while the output ring is priming or after a worker stall.

## Worker thread

The worker thread runs at `.userInteractive` quality of service, polls the
input ring with a `WorkerThreadEvent`, and processes complete chunks selected
by ``SpenchSettings/maxProcessingChunkSeconds``. Chunks are quantized to 16 ms
steps, bounded by ``SpenchSettings/minLatency`` and 256 ms, and have a 16 ms
minimum. The render callback can use any host-provided frame count; it does
not change the worker chunk duration. The worker aggregates per-chunk
processing time using a sliding-window mean. If the worker falls behind, the
unit emits a ``SpenchLowPerformanceEvent`` and bumps target latency in
``SpenchSettings/latencyIncreaseStep`` increments.

## Lifecycle rules

- Create ``SpenchSettings`` before instantiating the audio unit. Settings
  are validated and locked at allocation time.
- Treat settings as immutable for the lifetime of one audio-unit instance.
  Settings cannot be changed once the host has allocated render resources;
  recreate the audio unit if you need different settings.
- Feature changes are supported after creation through
  ``SpenchAVAudioUnit/setFeatures(_:)``. Pipeline rebuilds happen lazily
  the next time a worker chunk runs.
- ``SpenchAUAudioUnit/reset()`` rebuilds pipeline and ring-buffer state.
  Call it only when the engine is stopped or paused: the host owns the
  render thread and is not synchronised by reset, so concurrent renders
  during reset would race.
- ``SpenchAUAudioUnit/allocateRenderResources()`` rolls back to the
  previous valid state if any internal sub-step throws. The unit stays
  consistent on failure.
- Low-performance callbacks and notifications are delivered on the main
  queue.

## Direct processor concurrency

``SpenchProcessor`` is **not** safe for concurrent calls into
``SpenchProcessor/process(input:output:)``,
``SpenchProcessor/setFeatures(_:)``, or ``SpenchProcessor/reset()``.

Internal state mutation is locked, but the underlying ObjC++ pipeline
keeps scratch buffers per instance. Serialise calls from one thread, or
wrap the instance with your own synchronisation if you need to share it
across actors.

``SpenchSettings/maxProcessingChunkSeconds`` applies only to the real-time
audio-unit worker. Direct processor calls preserve the caller-provided buffer
boundaries.
