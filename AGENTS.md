# Spench repository guidance

These instructions apply to the whole repository. If a nested `AGENTS.md` is
added later, follow the more specific instructions for files in that subtree.

## Repository map

- `spench/`: C and C++ library sources, public headers, utilities, CLI tools,
  demos, and Catch2 tests.
- `spench/include/spench/`: supported public C and C++ SDK surface. Treat API
  and ABI compatibility here as intentional product constraints.
- `spench/contrib/`: vendored third-party code. Do not reformat or refactor it
  unless the task explicitly targets that dependency.
- `cmake/` and `CMakePresets.json`: build profiles, dependencies, packaging,
  platform selection, and compiler settings.
- `platform_wrappers/android/`, `platform_wrappers/ios/`, and
  `platform_wrappers/web/`: platform SDKs, demos, and platform-specific tests.
- `tools/spench`: canonical entry point for supported builds, tests, and
  packages. Prefer it to ad hoc CMake, Docker, Gradle, or npm command chains.
- `container_internals/`: style checks and container-side build helpers.
- `README.md`: setup, supported configurations, model retrieval, and user-facing
  examples.
- `STYLE.md`: authoritative C++ naming, formatting, API-stability, and lint
  rules.

## Working agreements

- Inspect `git status --short` before editing. Preserve user changes and do not
  revert, overwrite, or reformat unrelated work.
- Make the smallest coherent change that satisfies the request. Reuse existing
  abstractions and workflows before introducing new ones.
- Search with `rg` or `rg --files`; exclude generated and dependency trees from
  broad searches when practical.
- Do not edit generated output or cached dependencies in `build/`, `out/`,
  `.cpm/`, `.cache/`, `.cxx/`, `node_modules/`, `DerivedData/`, or generated
  XCFramework directories.
- Do not add or upgrade production dependencies unless the task requires it.
  Keep lockfiles synchronized with dependency-manifest changes.
- Never commit credentials, `.dvc/config`, private model URLs, or downloaded
  model binaries. Models are retrieved with DVC as described in `README.md`.
- Avoid destructive Git commands. Do not delete caches or build directories
  merely to work around a failure; diagnose the failure first.

## Canonical build and test interface

Run commands from the repository root. Docker is the default for native, WASM,
Android, and web targets; use `--host` only when the required local toolchain is
already installed. iOS workflows use the host Xcode toolchain.

```bash
# Fast native development validation
tools/spench test native-debug-onnx

# Release engine coverage
tools/spench test native-release-onnx
tools/spench test native-release-tflite

# Memory-safety validation
tools/spench test native-asan-onnx

# WebAssembly wrappers
tools/spench test wasm-release
tools/spench test wasm-dsp-release

# Full web SDK build, unit tests, and bundle
tools/spench build web

# Platform wrappers
tools/spench test android
tools/spench test ios

# Release packages
tools/spench package native-release-onnx
tools/spench package native-release-tflite
```

Dependency downloads and first-time container builds can be expensive. Start
with the narrowest relevant command and broaden coverage only when the change
crosses engines, platforms, ABI boundaries, or memory ownership.

## Validation matrix

- C or C++ implementation change: run `tools/spench test native-debug-onnx`.
- Inference-engine-neutral code or configuration: also run the relevant ONNX
  and TFLite release tests.
- Allocation, lifetime, threading, buffer, or ownership change: also run
  `tools/spench test native-asan-onnx`.
- WASM C++ or glue change: run the matching `wasm-release` test. Also run
  `wasm-dsp-release` when DSP-only behavior can be affected.
- Web SDK TypeScript-only change: from `platform_wrappers/web`, run
  `npm test --workspace=@gigaedge/spench-web-sdk` and the relevant build or
  lint script. Run `tools/spench build web` when generated WASM, staging,
  bundling, or the native/web boundary changes.
- Android or iOS wrapper change: run the matching platform workflow. Some
  Android instrumented tests require a device or emulator; document that
  limitation instead of claiming they ran.
- CMake, Docker, preset, packaging, or workflow change: exercise the exact
  affected preset or `tools/spench` action, not merely a source-level test.
- Documentation-only change: verify commands, links, and examples against the
  current repository; a full binary build is normally unnecessary.

If a required check cannot run because of missing models, credentials,
toolchains, a device, Docker, or network access, report the exact command and
blocker. Do not silently substitute a weaker check.

## C and C++ rules

- Follow `STYLE.md` and `.clang-format`. Internal functions and methods use
  `snake_case`; types use `PascalCase`; class members use a trailing underscore.
- Prefer fixed-width integer types in Spench code as documented in `STYLE.md`.
- Do not rename exported C symbols or existing public C++ API methods for style.
  Changes under `spench/include/spench/` require explicit compatibility review.
- Keep tests close to the affected behavior under `spench/tests/`. Add a
  regression test when fixing a reproducible bug.
- Run style validation for C/C++ changes:

  ```bash
  ./container_internals/check_style.sh
  ```

  Host-only naming checks require `clang-tidy` and `compile_commands.json`:

  ```bash
  ./container_internals/check_style.sh -m
  ```

## Web rules

- Treat `platform_wrappers/web` as an npm workspace. Run workspace commands
  from that directory unless an existing script says otherwise.
- Use `npm ci` for reproducible dependency installation; do not hand-edit
  `package-lock.json`.
- Keep Web SDK unit tests and demo Playwright tests distinct. A placeholder demo
  `npm test` is not evidence that browser behavior passed.
- Do not manually edit staged/generated WASM artifacts when the staging scripts
  or `tools/spench build web` can produce them.

## Completion criteria

Before reporting completion:

1. Review the diff for unintended generated files, binary changes, credentials,
   and unrelated formatting.
2. Run the smallest relevant checks from the matrix above.
3. State which checks passed and which were not run, including the reason.
4. Call out public API, ABI, model-format, packaging, or cross-platform impact.
