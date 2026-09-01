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

## Future GitHub CI/CD design

This section is the implementation blueprint for future files under
`.github/workflows/`. Keep workflow YAML thin: setup belongs in composite
actions and build behavior belongs in `tools/spench`, so local and CI commands
remain identical.

### Workflow boundaries

Create separate workflows with narrowly scoped permissions:

- `ci.yml`: `pull_request`, merge queue, and pushes to the protected default
  branch. It validates source but never publishes.
- `nightly.yml`: scheduled and manually dispatched exhaustive tests, including
  slow platform, sanitizer, browser, and device/emulator coverage.
- `dependencies.yml`: manually dispatched and triggered by dependency recipe
  changes. It prepares dependency bundles/images; it is not a product release.
- `release.yml`: manually dispatched or version-tag triggered. It packages,
  signs, verifies, and publishes only a commit that passed the full required CI
  gate.

Do not combine pull-request validation and publishing in one workflow. Forked
pull requests must never receive release secrets or write permissions.

### CI job order

Implement the dependency graph in this order:

```text
changes + policy
        |
        v
Linux native smoke test
        |
        +-------------------+-------------------+------------------+
        v                   v                   v                  v
native/engine matrix   WASM + web matrix   Android matrix   macOS/Windows/iOS
        +-------------------+-------------------+------------------+
                                |
                                v
                     install/package verification
                                |
                                v
                       single required CI gate
```

1. **Changes and policy.** Classify affected areas (`core`, `build`, `web`,
   `android`, `ios`, `docs`) and validate repository metadata. Use path
   classification to avoid expensive unrelated jobs on pull requests, but do
   not skip the full matrix on the default branch, nightly runs, or releases.
   Use shallow checkout for lightweight jobs. Jobs that execute tests must
   fetch Git LFS objects because models and audio fixtures are LFS-managed.
2. **Linux native smoke test.** Run the cheapest representative end-to-end
   build first, normally `tools/spench test native-debug-onnx`. Expensive jobs
   should depend on this job so basic checkout, Docker, CMake, cache, model, and
   test failures stop the pipeline early.
3. **Parallel validation matrices.** After smoke succeeds, run independent
   jobs concurrently with matrix `fail-fast: false` so all platform failures
   are reported in one run:
   - Linux native: Debug and Release for ONNX Runtime and TensorFlow Lite, plus
     `native-asan-onnx` and the supported static-release variants.
   - WASM: `wasm-debug`, `wasm-release`, `wasm-dsp-debug`, and
     `wasm-dsp-release`.
   - Web: `tools/spench build web`; run demo lint and Playwright separately so
     a placeholder package test cannot satisfy browser coverage.
   - Android: `tools/spench test android`; keep connected emulator tests in
     nightly CI until runner reliability and duration are acceptable for pull
     requests.
   - macOS: run only presets matching the runner architecture, verify it with
     `uname -m`, and include the iOS simulator workflow on a compatible Xcode
     runner.
   - Windows: invoke the helper as `python tools/spench test <preset>` and test
     both engines plus the supported static variant.
4. **Install and package verification.** Run only after the corresponding test
   job passes. Exercise `tools/spench install <preset>` for distributable CMake
   installs and `tools/spench package <preset>` for supported release archives.
   Add a small external consumer project that uses the installed package; a
   successful archive command alone does not prove consumers can find headers,
   libraries, transitive dependencies, or CMake package metadata.
5. **Single required gate.** Add one final job with stable name such as
   `ci-required`. It must use `if: always()`, inspect every required dependency,
   and fail when a required job failed, was cancelled, or was unexpectedly
   skipped. Configure branch protection against this one name so later matrix
   expansion does not require branch-protection changes.

Documentation-only pull requests may stop after policy/documentation checks.
Changes to `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `Dockerfile`,
`package-lock.cmake`, `tools/spench`, dependency patches, public headers, or
cross-platform implementation code must select broad coverage.

### Cache and dependency-bundle policy

- Immediately after checkout, call
  `./.github/actions/setup-spench-cache` once per build job with that job's
  exact preset. Do not duplicate cache-key logic in workflow YAML.
- Treat CPM and compiler caches only as accelerators. A cache miss must remain a
  correct build, and release artifacts must never be published directly from a
  mutable cache.
- Build dependency bundles with `tools/spench prepare-deps <target>` and export
  dependency-only OCI archives with `tools/spench prepare-image <family>`.
- Publish dependency images only from trusted default-branch or manually
  approved runs. Address them by immutable digest or dependency-lock-derived
  tag, validate their `manifest.json`, and fall back to a source build when no
  compatible bundle exists.
- Rebuild bundles when `package-lock.cmake`, CPM configuration, dependency
  patches, relevant CMake recipes, toolchain identity, or bundle format changes.
- Use concurrency cancellation for superseded pull-request runs. Do not cancel
  release or dependency-publication workflows halfway through publishing.

### Artifacts and diagnostics

- On failure, upload CTest logs, CMake configure logs, compiler-cache statistics,
  Playwright reports/traces, Gradle reports, and Xcode result bundles when they
  exist. Keep failure artifacts for a limited debugging retention period.
- On success, upload only deliberate products from `out/artifacts/`, together
  with a manifest and SHA-256 checksums. Never upload the entire workspace,
  dependency caches, credentials, or `.dvc/config`.
- Put a compact matrix and cache summary in `GITHUB_STEP_SUMMARY`; job logs
  remain the detailed source of truth.

### CD and release order

Release from the exact tested tag commit, never by rebuilding an unpinned
branch. Use a protected GitHub Environment for approvals and secrets. Prefer
short-lived/OIDC credentials where the destination supports them.

1. Validate the version/tag and confirm the commit passed `ci-required` with
   the full release matrix.
2. Build packages independently for Linux, macOS, Windows, Android, iOS, and
   WASM using the same helper commands as CI.
3. Verify each install/package with a consumer or smoke test, then generate
   checksums and provenance/SBOM metadata. Sign and notarize where applicable.
4. Publish immutable native/platform artifacts and the validated WASM artifacts
   first.
5. Stage the exact published WASM outputs into the Web SDK, rerun Web SDK unit,
   build, and browser tests, and only then publish the npm package. Do not build
   a different WASM binary during the Web SDK release.
6. Create or finalize the GitHub Release only after every required publication
   succeeds. Record all artifact hashes and versions in the release notes.

A rerun must be idempotent: it may verify an already published artifact with
the same hash, but it must fail rather than overwrite an existing version with
different bytes.

### Known prerequisites before CI becomes required

- `.pre-commit-config.yaml`, `STYLE.md`, and this file reference
  `container_internals/check_style.sh`, but `container_internals/` is not
  present in the current repository. Restore those scripts or update all three
  references before adding a required style job.
- Decide the authoritative release version source and tag format before
  implementing `release.yml`; the current `README.md` release section is marked
  for rewrite.

## Completion criteria

Before reporting completion:

1. Review the diff for unintended generated files, binary changes, credentials,
   and unrelated formatting.
2. Run the smallest relevant checks from the matrix above.
3. State which checks passed and which were not run, including the reason.
4. Call out public API, ABI, model-format, packaging, or cross-platform impact.
