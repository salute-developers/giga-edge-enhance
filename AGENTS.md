# GigaEdge Enhance repository guidance

These instructions apply to the whole repository. If a nested `AGENTS.md` is
added later, follow the more specific instructions for files in that subtree.

The official product name is GigaEdge Enhance. Use that spaced form in
user-facing documentation. Code identifiers use the `GigaEdgeEnhance` prefix,
the C++ namespace `giga_edge::enhance`, the include directory `enhance/`, the
CMake target `giga-edge-enhance`, the `GEE_` prefix, and the `.enhance` packed-model
extension. The feature class remains `Denoiser`. The GitHub repository slug
is `giga-edge-enhance`.

C++ classes use short names in
`giga_edge::enhance`; C symbols use `GigaEdgeEnhance`, the installed CMake target is
`giga-edge::enhance`, and the Swift module/types use `EnhanceAudioKit` / `Enhance*`.
The repository slug and public dependency revisions stay unchanged.
GigaEdge Enhance starts at 1.0.0 across all platforms so the Git tag, packages
and model catalog agree on the first public SDK version.

The umbrella C++ header is `giga-edge-enhance.h`. The native library and CMake
package use `giga-edge-enhance`; C++ uses `giga_edge` for its root namespace.
Public documentation describes the current SDK interface. Internal branding
history and migration notes belong in private team documentation, not in the
public documentation or release notes.

## Repository map

- `core/`: C and C++ library sources, public headers, utilities, CLI tools,
  demos, and Catch2 tests.
- `core/include/enhance/`: supported public C and C++ SDK surface. Treat API
  and ABI compatibility here as intentional product constraints.
- `package-lock.cmake` and `out/cache/cpm/`: pinned dependency declarations and
  their generated source cache. Do not edit cached dependency sources.
- `cmake/` and `CMakePresets.json`: build profiles, dependencies, packaging,
  platform selection, and compiler settings.
- `android/`, `ios/`, and `web/`: platform SDKs, demos, and platform-specific
  tests.
- `tools/`: canonical command suite for supported builds, tests, installs, and
  packages. See `tools/README.md`; prefer it to ad hoc toolchain command chains.
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
- Public quickstarts use user-provided recordings. Keep listening-demo audio
  out of the source tree so these examples do not depend on bundled demo media.
- Avoid destructive Git commands. Do not delete caches or build directories
  merely to work around a failure; diagnose the failure first.
- Record durable product, platform, release, and CI decisions in this file in
  the same change, including both the decision and its rationale.

## Canonical build and test interface

Run commands from the repository root. Docker is the required default for
native, WASM, Android, and web builds and tests. Do not pass `--host` during
routine development or validation; host mode is retained only for explicit
manual troubleshooting. CI and reported validation must use the commands
without `--host` so toolchain versions remain reproducible. iOS workflows and
platform-specific macOS and Windows presets use their required host toolchains
implicitly.

```bash
# Fast native development validation
tools/build native-debug-onnx
tools/test native-debug-onnx

# Release engine coverage
tools/build native-release-onnx
tools/test native-release-onnx
tools/build native-release-tflite
tools/test native-release-tflite

# Memory-safety validation
tools/build native-asan-onnx
tools/test native-asan-onnx

# WebAssembly wrappers
tools/build wasm-release
tools/test wasm-release
tools/build wasm-dsp-release
tools/test wasm-dsp-release

# Full web SDK build, unit tests, and bundle
tools/build web
tools/test web

# Platform wrappers
tools/build android
tools/test android
tools/build ios
tools/test ios

# Release packages
tools/build native-release-onnx
tools/package native-release-onnx
tools/build native-release-tflite
tools/package native-release-tflite
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
  `npm test --workspace=@gigaedge/enhance-web-sdk` and the relevant build or
  lint script. Run `tools/build web` when generated WASM, staging,
  bundling, or the native/web boundary changes.
- Android or iOS wrapper change: run the matching platform workflow. Some
  Android instrumented tests require a device or emulator; document that
  limitation instead of claiming they ran.
- CMake, Docker, preset, packaging, or workflow change: exercise the exact
  affected preset or `tools` action, not merely a source-level test.
- Documentation-only change: verify commands, links, and examples against the
  current repository; a full binary build is normally unnecessary.

If a required check cannot run because of missing models, credentials,
toolchains, a device, Docker, or network access, report the exact command and
blocker. Do not silently substitute a weaker check.

## C and C++ rules

- Follow `STYLE.md` and `.clang-format`. Internal functions and methods use
  `snake_case`; types use `PascalCase`; class members use a trailing underscore.
- Prefer fixed-width integer types in GigaEdge Enhance code as documented in `STYLE.md`.
- Do not rename exported C symbols or existing public C++ API methods for style.
  Changes under `core/include/enhance/` require explicit compatibility review.
- Keep tests close to the affected behavior under `core/tests/`. Add a
  regression test when fixing a reproducible bug.
- Add native behavior tests to the existing `enhance_tests` Catch2 unit-test suite.
- Check changed C/C++ lines against `.clang-format`:

  ```bash
  git-clang-format --diff HEAD --extensions c,cc,cpp,h,hpp -- core
  ```

  The required CI lint uses the same changed-line check. Broader legacy style
  cleanup should be done independently rather than mixed into feature changes.

## Python SDK

- The distribution is `giga-edge-enhance`, imported as `giga_edge_enhance`. Keep public usage in `python/readme.md`, API semantics in `python/reference.md`, and build commands in `tools/README.md`.
- Reuse the native core. ONNX, TFLite and DSP live in separate extensions with hidden symbols and module-local types. Each stream owns its mutable state. Drain input resampling before finalizing the denoiser.
- Models are separate from wheels. Verify weights and YAML against the SDK catalog when loading local or Hub exports. Keep native fixtures and test executables out of distributions.
- Take the package version from `VERSION_NUMBER`. Test installed, repaired wheels with independent C++ consumers on each supported platform. Keep manylinux dependency bundles separate from native Linux builds. Publish Python release artifacts only after all four platforms pass.

## Web rules

- Treat `web` as an npm workspace. Run workspace commands
  from that directory unless an existing script says otherwise.
- Use `npm ci` for reproducible dependency installation; do not hand-edit
  `package-lock.json`.
- Keep Web SDK unit tests and demo Playwright tests distinct. A placeholder demo
  `npm test` is not evidence that browser behavior passed.
- Do not manually edit staged/generated WASM artifacts when the staging scripts
  or `tools/build web` can produce them.

## GitHub CI/CD design

Keep workflow YAML thin: setup belongs in composite actions and build behavior
belongs in `tools`, so local and CI commands remain identical.

Dependabot checks GitHub Actions, npm, Gradle, Docker, and pip dependencies monthly. Group compatible
minor and patch updates to limit pull-request noise, while keeping major updates separate for focused
review. CMake/CPM dependencies remain manual because Dependabot does not support the repository's
`package-lock.cmake` workflow.

Commit linting extends the Conventional Commits preset and retains the
100-character header limit. Commit bodies intentionally have no line-length
limit so URLs, trailers, and explanatory prose do not require history rewrites.
Dependabot uses `build` with its generated `deps` or `deps-dev` scope so its
commits pass the same rules as contributor commits.

### Workflow boundaries

Workflows have event-specific responsibilities and narrowly scoped permissions:

- `ci.yml`: pull requests and pushes to the protected default branch. It uses
  Release configurations only, uploads tested job artifacts, and never deploys.
- `ci.yml` publishes same-repository pull-request previews immediately after the
  Web build and unit tests. `pages.yml` publishes the default-branch artifact,
  while `preview-cleanup.yml` handles only a closed pull request. Derive the
  site's base URL from the repository name and append `pr/<number>/` for
  previews so asset URLs match their GitHub Pages publication paths.
  Only successful default-branch pushes to this repository may enter the
  Pages deployment concurrency group and publish the main site. This keeps
  pull-request artifacts and cancellations isolated from production.
- `release-assets.yml` and `release-please.yml`: release-only packaging and
  publication. They never run as pull-request checks.

Platform jobs are enabled by default. Set the GitHub Actions repository variable
`ENABLE_MACOS_JOBS` or `ENABLE_WINDOWS_JOBS` to `false` to skip jobs requiring
that runner platform before runner allocation. `ENABLE_MACOS_JOBS` also controls
iOS jobs because they require macOS runners. Disabled platform jobs do not
produce release artifacts, so release publication remains blocked until every
platform is enabled and its required artifacts exist.

Set `ENABLE_ARTIFACT_UPLOADS=false` to run builds and unit tests without Actions
artifact uploads when storage is constrained. Web Playwright, PR previews,
default-branch Pages deployment, and release automation are skipped because
they consume those artifacts. Docker Bake build records are always disabled;
toolchain images and dependency bundles still use GHCR independently.

Do not combine pull-request validation and publishing in one workflow. Forked
pull requests must never receive release secrets or write permissions.
Run Python lint from external pull requests on a GitHub-hosted runner with
read-only contents access and disable checkout credential persistence.

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
  Debug, ASAN, and static presets remain local `tools` workflows rather
  than manually dispatched GitHub Actions jobs.
- Web owns both full and DSP-only WASM builds. Web SDK unit tests and Chromium
  Playwright coverage run in the Linux combined Web toolchain image. The
  required CI gate is intentionally Chromium-only to keep browser validation
  on one reproducible engine. Local `tools/test web-playwright` runs the
  Chromium, Firefox, and WebKit projects by default; set
  `GEE_PLAYWRIGHT_BROWSERS` to a comma-separated subset for a focused run.
  Playwright consumes the packed SDK from the Web build job and runs against
  the local COOP/COEP server. Forward `CI` into the Playwright container so its
  two configured retries are active in CI. Linux MOS results remain
  authoritative, but processed-audio checks allow 10% regression while source
  controls retain 5% because browser recording scores vary by host media stack.
- Android restores one TFLite dependency per ABI and runs Release unit/native
  and connected emulator tests. Debug validation remains a local workflow.
- macOS validates each runner architecture with both engines. Windows validates
  both shared-engine targets. iOS restores both device and
  simulator dependencies before producing and testing the XCFramework.
- The iOS SDK build enables `install-xcodegen` in `setup-host-toolchain` because
  GitHub-hosted macOS runners do not include XcodeGen. Dependency-only and desktop
  macOS jobs leave it disabled because they do not generate the iOS SDK project.
- iOS tests select an available iPhone from the runtime matching the selected
  Xcode simulator SDK, not a hard-coded device from a potentially newer runtime.
  Preparation logs each command and boot progress, fails on errors, and limits
  each command to three minutes to avoid silent CoreSimulator hangs in CI.
- Lint is split by domain: C/C++, Web, Android, iOS, and workflow YAML.

### Cache and dependency-bundle policy

- Immediately after checkout, call
  `./.github/actions/setup-cache` once per build job with that job's
  exact preset. Do not duplicate cache-key logic in workflow YAML.
- Treat CPM and compiler caches only as accelerators. A cache miss must remain a
  correct build, and release artifacts must never be published directly from a
  mutable cache.
- ONNX Runtime uses its pinned FetchContent Eigen revision. Disable its optional
  `find_package(Eigen3)` lookup so a user's CMake package registry cannot inject
  headers from an unrelated or still-incomplete TFLite build.
- With GCC 14+, keep `maybe-uninitialized` as a visible warning on the pinned
  ONNX optimizer target. Its libstdc++ variant/function copy path triggers this
  diagnostic under optimization; do not weaken warnings on SDK-owned targets.
- Build one dependency bundle with `tools/ci prepare-deps <concrete-target>`.
  Fetch public dependency sources without a personal token so a cache miss
  can be built in Dependabot CI. Use the job's `GITHUB_TOKEN` for GHCR access.
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
  exist. Keep diagnostic artifacts for one day when uploads are enabled.
- On success, upload only deliberate products from `out/artifacts/`, together
  with a manifest and SHA-256 checksums. Never upload the entire workspace,
  dependency caches, credentials, or private artifact configuration.
- Retain PR products for `PR_ARTIFACT_RETENTION_DAYS` (default one day) and
  default-branch products for `MASTER_ARTIFACT_RETENTION_DAYS` (default three
  days). These GitHub Actions repository variables are whole days, at least one
  day; complete release promotion within the default-branch retention window.
  Web site artifacts remain at one day for Pages and PR previews.
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

Resolve draft releases through the paginated releases list. GitHub's
release-by-tag endpoint returns published releases only, so it cannot locate the
draft that must receive all verified assets before publication.

`VERSION_NUMBER` is the authoritative release version source. Its name must not
be shortened to `VERSION`: on case-insensitive filesystems that filename shadows
libc++'s standard `<version>` header when the source root is an include path.

## Completion criteria

Before reporting completion:

1. Review the diff for unintended generated files, binary changes, credentials,
   and unrelated formatting.
2. Run the smallest relevant checks from the matrix above.
3. State which checks passed and which were not run, including the reason.
4. Call out public API, ABI, model-format, packaging, or cross-platform impact.

## ONNX Runtime lifetime

Keep the lazily initialized default `Ort::Env` alive for the process lifetime.
Do not register an SDK destructor that calls `ReleaseEnv`: ORT's static mutex
may already be destroyed during process teardown. Initialization must be
thread-safe, and temporary threading options must still be released on failure
and success.

## Hybrid inference

Hybrid models use the `inference.model_type: denoiser_hybrid_web_model` configuration with FP32 ONNX and
TFLite tensors. Their output is an enhanced complex spectrum, so it must bypass mask application.
Keep recurrent states and SNR averaging windows local to each stream context to avoid cross-stream
state contamination. Hybrid coverage uses reproducible identity fixtures generated by
`core/tests/generate_hybrid_fixtures.py` and the `hybrid48` model assets.

Model identifiers are `denoiser_discr_hq_model` (8 kHz quantized ONNX), `denoiser_discr_web_model`
(discriminative 48 kHz FP32), and `denoiser_hybrid_web_model` (hybrid 48 kHz FP32). Default Web denoising is
the discriminative 48 kHz model. Distribute `hybrid48` through the same platform model resources as `quality48`.
Apply the decompressed `quality48` mask by complex multiplication to match training, capping its magnitude at one.
Repack public model assets whenever their embedded configuration changes, preserving
and verifying the exact network payload; the packed file format itself remains unchanged.
