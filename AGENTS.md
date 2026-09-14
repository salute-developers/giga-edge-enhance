# Spench repository guidance

These instructions apply to the whole repository. If a nested `AGENTS.md` is
added later, follow the more specific instructions for files in that subtree.

## Repository map

- `spench/`: C and C++ library sources, public headers, utilities, CLI tools,
  demos, and Catch2 tests.
- `spench/include/spench/`: supported public C and C++ SDK surface. Treat API
  and ABI compatibility here as intentional product constraints.
- `package-lock.cmake` and `out/cache/cpm/`: pinned dependency declarations and
  their generated source cache. Do not edit cached dependency sources.
- `cmake/` and `CMakePresets.json`: build profiles, dependencies, packaging,
  platform selection, and compiler settings.
- `android/`, `ios/`, and `web/`: platform SDKs, demos, and platform-specific
  tests.
- `tools/spench`: canonical entry point for supported builds, tests, and
  packages. Prefer it to ad hoc CMake, Docker, Gradle, or npm command chains.
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
- Never commit credentials, private model URLs, or downloaded model binaries.
  Repository model and media assets are restored through Git LFS as described
  in `README.md`; there is no DVC or submodule setup step.
- Avoid destructive Git commands. Do not delete caches or build directories
  merely to work around a failure; diagnose the failure first.
- Record durable product, platform, release, and CI decisions in this file in
  the same change, including both the decision and its rationale.

## Canonical build and test interface

Run commands from the repository root. Docker is the default for native, WASM,
Android, and web targets; use `--host` only when the required local toolchain is
already installed. iOS workflows use the host Xcode toolchain.

```bash
# Fast native development validation
tools/spench build native-debug-onnx
tools/spench test native-debug-onnx

# Release engine coverage
tools/spench build native-release-onnx
tools/spench test native-release-onnx
tools/spench build native-release-tflite
tools/spench test native-release-tflite

# Memory-safety validation
tools/spench build native-asan-onnx
tools/spench test native-asan-onnx

# WebAssembly wrappers
tools/spench build wasm-release
tools/spench test wasm-release
tools/spench build wasm-dsp-release
tools/spench test wasm-dsp-release

# Full web SDK build, unit tests, and bundle
tools/spench build web
tools/spench test web

# Platform wrappers
tools/spench build android
tools/spench test android
tools/spench build ios
tools/spench test ios

# Release packages
tools/spench build native-release-onnx
tools/spench package native-release-onnx
tools/spench build native-release-tflite
tools/spench package native-release-tflite
```

Dependency downloads and first-time container builds can be expensive. Start
with the narrowest relevant command and broaden coverage only when the change
crosses engines, platforms, ABI boundaries, or memory ownership.

## Validation matrix

- C or C++ implementation change: build, then test `native-debug-onnx`.
- Inference-engine-neutral code or configuration: also run the relevant ONNX
  and TFLite release tests.
- Allocation, lifetime, threading, buffer, or ownership change: also build and
  test `native-asan-onnx`.
- WASM C++ or glue change: build and test the matching `wasm-release` target.
  Also build and test `wasm-dsp-release` when DSP-only behavior can be affected.
- Web SDK TypeScript-only change: from `web`, run
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
- Check changed C/C++ lines against `.clang-format`:

  ```bash
  git-clang-format --diff HEAD --extensions c,cc,cpp,h,hpp -- spench
  ```

  The required CI lint uses the same changed-line check. Broader legacy style
  cleanup should be done independently rather than mixed into feature changes.

## Web rules

- Treat `web` as an npm workspace. Run workspace commands
  from that directory unless an existing script says otherwise.
- Use `npm ci` for reproducible dependency installation; do not hand-edit
  `package-lock.json`.
- Keep Web SDK unit tests and demo Playwright tests distinct. A placeholder demo
  `npm test` is not evidence that browser behavior passed.
- Do not manually edit staged/generated WASM artifacts when the staging scripts
  or `tools/spench build web` can produce them.

## GitHub CI/CD design

Keep workflow YAML thin: setup belongs in composite actions and build behavior
belongs in `tools/spench`, so local and CI commands remain identical.

### Workflow boundaries

Workflows have event-specific responsibilities and narrowly scoped permissions:

- `ci.yml`: pull requests and pushes to the protected default branch. It uses
  Release configurations only, uploads tested job artifacts, and never deploys.
- `ci.yml` publishes same-repository pull-request previews immediately after the
  Web build and unit tests. `pages.yml` publishes the default-branch artifact,
  while `preview-cleanup.yml` handles only a closed pull request.
- `release-assets.yml` and `release-please.yml`: release-only packaging and
  publication. They never run as pull-request checks.

Do not combine pull-request validation and publishing in one workflow. Forked
pull requests must never receive release secrets or write permissions.

### CI job order

For each concrete target, implement this graph:

```text
Build toolchain -> Build concrete dependency OCI -> Build and test target
```

Dependency jobs run in parallel. As soon as one exact dependency finishes, its
target may start; do not put unrelated dependencies behind a matrix-wide
barrier. Native, Web, and Android do not depend on each other. Build and test
remain separate named steps in one job so large build trees do not have to be
uploaded and downloaded between runners.

- Linux CI validates ONNX Runtime and TensorFlow Lite shared Release targets.
  Debug, ASAN, and static presets remain local `tools/spench` workflows rather
  than manually dispatched GitHub Actions jobs.
- Web owns both full and DSP-only WASM builds. Web SDK unit tests and Chromium
  Playwright coverage run in the Linux combined Web toolchain image. The
  required CI gate is intentionally Chromium-only to keep browser validation
  on one reproducible engine. Local `tools/spench test web-playwright` runs the
  Chromium, Firefox, and WebKit projects by default; set
  `SPENCH_PLAYWRIGHT_BROWSERS` to a comma-separated subset for a focused run.
  Playwright consumes the packed SDK from the Web build job and runs against
  the local COOP/COEP server. Linux MOS results remain authoritative because
  scores vary by host media stack.
- Android restores one TFLite dependency per ABI and runs Release unit/native
  and connected emulator tests. Debug validation remains a local workflow.
- macOS validates each runner architecture with both engines. Windows validates
  both shared-engine targets. iOS restores both device and
  simulator dependencies before producing and testing the XCFramework.
- Lint is split by domain: C/C++, Web, Android, iOS, and workflow YAML.

### Cache and dependency-bundle policy

- Immediately after checkout, call
  `./.github/actions/setup-spench-cache` once per build job with that job's
  exact preset. Do not duplicate cache-key logic in workflow YAML.
- Treat CPM and compiler caches only as accelerators. A cache miss must remain a
  correct build, and release artifacts must never be published directly from a
  mutable cache.
- Build one dependency bundle with `tools/spench prepare-deps <concrete-target>`.
  Package it as a checksummed tar archive and push it to GHCR as an OCI artifact
  with ORAS. Do not wrap dependency files in a runnable Docker image.
- Tag toolchain images by Dockerfile/toolchain content, never by commit SHA.
  Address dependency OCI artifacts by engine recipe, target OS/architecture,
  configuration, linkage, bundle format, and immutable toolchain identity.
- Reuse matching OCI artifacts across pull requests and default-branch builds.
  Validate the checksum and `manifest.json` fields before exposing a restored
  prefix to CMake.
- Rebuild bundles when `package-lock.cmake`, CPM configuration, dependency
  patches, relevant CMake recipes, toolchain identity, or bundle format changes.
- Use concurrency cancellation for superseded pull-request runs. Do not cancel
  a release halfway through publication.

### Artifacts and diagnostics

- On failure, upload CTest logs, CMake configure logs, compiler-cache statistics,
  Playwright reports/traces, Gradle reports, and Xcode result bundles when they
  exist. Keep CMake/CTest failure artifacts for three days.
- On success, upload only deliberate products from `out/artifacts/`, together
  with a manifest and SHA-256 checksums. Never upload the entire workspace,
  dependency caches, credentials, or private artifact configuration.
- Retain tested artifacts that can feed a release for seven days so the
  asynchronous release workflow is not constrained to a one-day promotion
  window.
- Put a compact matrix and cache summary in `GITHUB_STEP_SUMMARY`; job logs
  remain the detailed source of truth.

### CD and release order

Release from the exact tested tag commit, never by rebuilding an unpinned
branch. Release publication uses only the repository `GITHUB_TOKEN`, so it does
not use a protected GitHub Environment; add an Environment only if a future
external destination introduces secrets or an explicit approval requirement.

1. Validate the version/tag and confirm the commit passed every required check
   configured in branch protection.
2. Build packages independently for Linux, macOS, Windows, Android, iOS, and
   WASM using the same helper commands as CI.
3. Verify each install/package with a consumer or smoke test, then generate
   checksums and provenance/SBOM metadata. Sign and notarize where applicable.
4. Create a draft GitHub Release and attach the exact validated native,
   platform, WASM, and Web SDK archives. GitHub release assets are the only
   publication destination; do not publish the Web SDK to npm.
5. Publish the GitHub Release only after every required attachment succeeds.
   Record all artifact hashes and versions in the release notes.

A rerun must be idempotent: it may verify an already published artifact with
the same hash, but it must fail rather than overwrite an existing version with
different bytes.

`SPENCH_VERSION` is the authoritative release version source. Its name must not
be shortened to `VERSION`: on case-insensitive filesystems that filename shadows
libc++'s standard `<version>` header when the source root is an include path.

## Completion criteria

Before reporting completion:

1. Review the diff for unintended generated files, binary changes, credentials,
   and unrelated formatting.
2. Run the smallest relevant checks from the matrix above.
3. State which checks passed and which were not run, including the reason.
4. Call out public API, ABI, model-format, packaging, or cross-platform impact.
