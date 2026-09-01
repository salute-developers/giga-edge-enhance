# Spench Web SDK workspace

This npm workspace contains the distributable Web SDK and the combined documentation/demo website.

```text
packages/web-sdk/          Web Audio and WebAssembly SDK
services/spench-demo/      Docusaurus documentation and interactive demo
```

## Requirements

Use the Node.js version declared in `.nvmrc`:

```bash
cd platform_wrappers/web
nvm use
npm ci
```

The Docker-backed `tools/spench build web` workflow selects this version automatically. A host build
uses NVM when available and otherwise leaves the active host Node.js version unchanged.

## Build the Web SDK

From the repository root:

```bash
tools/spench build web
```

This builds the full and DSP-only release WebAssembly variants, validates and stages their outputs,
runs Web SDK unit tests, and builds the package and static website.

To run individual stages:

```bash
tools/spench build wasm-release
tools/spench build wasm-dsp-release

cd platform_wrappers/web
npm run stage:wasm:release
npm run test:web-sdk
npm run build:web-sdk
npm run build --workspace=spench-demo
```

Generated WebAssembly assets live under `packages/web-sdk/generated/wasm/` and are not committed.
`npm pack` validates the release manifest before packaging.

## Develop the documentation and demo

```bash
cd platform_wrappers/web
npm start --workspace=spench-demo
```

The demo imports the workspace SDK during development and packs the SDK before a production build.
See [`services/spench-demo/README.md`](services/spench-demo/README.md) for testing and GitHub Pages
details.

## Git LFS

Install Git LFS once, then restore the repository's managed binary fixtures:

```bash
git lfs install
git lfs checkout
```

When adding new binary test fixtures, update `.gitattributes` with `git lfs track` and commit both the
attributes change and the resulting LFS pointer. Never commit downloaded model binaries outside the
repository's established model workflow.
