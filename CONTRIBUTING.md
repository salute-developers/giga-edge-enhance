# Contributing to GigaEdge Enhance

## Quickstart

Fork or clone the repository, then restore Git LFS content:

```bash
git lfs install
git lfs checkout
```

Run the fast native validation from the repository root:

```bash
tools/build native-debug-onnx
tools/test native-debug-onnx
```

Docker is the default and provides the supported reproducible toolchain. Host
mode exists for explicit manual troubleshooting, but normal development and
reported validation must run commands without `--host`.

See [`tools/README.md`](tools/README.md) for every command and target.

Use the repository's Discussions for integration and usage questions. Open an issue for a
reproducible bug or a concrete feature proposal. See [`SUPPORT.md`](SUPPORT.md) for channel and
support expectations. Report suspected vulnerabilities privately according to
[`SECURITY.md`](SECURITY.md).

## Requirements

- Git and Git LFS.
- Docker for the default native Linux, WebAssembly, Web, and Android workflows.
- Xcode and XcodeGen for iOS and macOS work.
- Visual Studio 2022 for Windows work.
- Node.js matching `web/.nvmrc` for host Web development.
- An Android SDK and NDK matching the repository configuration for host Android work.

Do not commit credentials, private model URLs, downloaded model binaries, build
output, dependency caches, or generated platform products.

Generated trees under `out/`, `build/`, `.cpm/`, and `.cache/` are not source
files. Do not edit dependency sources under `out/cache/cpm/`; update
`package-lock.cmake` or the corresponding patch under `cmake/patches/` instead.
Keep generated-file boundaries intact and commit lockfile updates with their
manifest changes.

## Repository Layout

- `core/`: native C and C++ SDK, implementation, utilities, CLI, and tests.
- `core/include/enhance/`: compatibility-sensitive public headers.
- `android/`: Kotlin SDK, JNI bridge, models, and demo application.
- `ios/`: Swift and Objective-C++ framework, model bundles, tests, and demo.
- `web/`: Web SDK npm workspace, WebAssembly bridge, documentation, and demo.
- `cmake/` and `CMakePresets.json`: build profiles, dependencies, and packaging.
- `tools/`: supported local and CI command interfaces.
- `.github/`: CI, release workflows, and composite actions.

## Choose Validation

Run the narrowest relevant checks first, then broaden them when a change crosses
platform or ABI boundaries.

| Change | Required validation |
| --- | --- |
| Native C/C++ | `tools/build native-debug-onnx`, then `tools/test native-debug-onnx` |
| Engine-neutral inference | Add ONNX and TFLite Release builds and tests |
| Memory ownership or threading | Add `native-asan-onnx` build and test |
| Public CMake/install surface | Build and run `tools/install` for the affected preset |
| Native packaging | Build, test, install, and package the affected Release preset |
| WebAssembly | Build and test the affected `wasm-*` target |
| Web SDK | `tools/build web`, `tools/test web`, and relevant lint/browser tests |
| Android | `tools/build android` and `tools/test android`; run UI tests when applicable |
| iOS | `tools/build ios` and `tools/test ios`; run `ios/scripts/lint.sh` |
| Workflow or composite action | Infrastructure tests and `actionlint` |
| Documentation | Verify commands, links, paths, and examples |

If a check cannot run because of a missing toolchain, device, Docker, network,
credential, or model, report the exact command and blocker. Do not silently
substitute a weaker check.

## Code Style

Follow [`STYLE.md`](STYLE.md) and the existing platform conventions.

- C++ implementation functions and methods use `snake_case`.
- C++ types use `PascalCase`; members use a trailing underscore.
- Public C symbols retain their `GigaEdge Enhance*` prefix because C has no namespace.
- Public C++ code uses the `gigaedge::enhance` namespace.
- Kotlin public types rely on `com.gigaedge.enhance` for product context.
- Swift public types rely on the `EnhanceAudioKit` module for product context.
- Web public exports retain `GigaEdgeEnhanceSDK`, `GigaEdgeEnhanceAudioNode`, and
  `GigaEdgeEnhanceError`.

Check changed C and C++ lines with:

```bash
git-clang-format --diff HEAD --extensions c,cc,cpp,h,hpp -- core android ios web
```

Do not reformat unrelated legacy code in a focused contribution.

## Tests and Documentation

Add regression coverage for reproducible bugs and tests for new behavior. Keep
native tests under `core/tests/` and platform tests with their SDK.

Update user-facing documentation whenever commands, public APIs, package names,
configuration, platform behavior, or release behavior changes. Update
`tools/README.md` whenever the command matrix changes.

## Commits and Pull Requests

- Use Conventional Commit messages consistent with repository history.
- Keep commit headers at or below 100 characters. Commit bodies have no line-length limit.
- Keep commits focused and do not include generated output or unrelated cleanup.
- Mark intentional incompatible changes with `!` and explain migration in the body.
- Rebase on the current default branch before requesting final review.
- Describe behavior changes, API or ABI impact, and validation results in the pull request.
- Link the relevant issue when one exists.
- Never bypass hooks or disable required checks to make a pull request pass.

No contributor license agreement or Developer Certificate of Origin sign-off is required.
Contributions are submitted under the repository's MIT License.

Release publication is performed by repository workflows from tested default-
branch commits. Contributors should not upload release assets or publish npm,
Android, or Apple packages manually.

## Security

Do not open a public issue for a suspected vulnerability. Follow
[`SECURITY.md`](SECURITY.md) to report it privately.
