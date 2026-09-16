# GigaDenoiser Web SDK workspace

This npm workspace contains the distributable Web SDK and the combined documentation/demo website.

```text
packages/web-sdk/          Web Audio and WebAssembly SDK
apps/site/          Docusaurus documentation and interactive workbench
```

## Requirements

Use the Node.js version declared in `.nvmrc`:

```bash
cd web
nvm use
npm ci
```

The Docker-backed `tools/build web` workflow selects this version automatically. A host build
uses NVM when available and otherwise leaves the active host Node.js version unchanged.

## Build the Web SDK

From the repository root:

```bash
tools/build web
```

This builds the full and DSP-only release WebAssembly variants, validates and stages their outputs,
runs Web SDK unit tests, and builds the package and static website.

To run individual stages:

```bash
tools/build wasm-release
tools/build wasm-dsp-release

cd web
npm run stage:wasm:release
npm run test:web-sdk
npm run build:web-sdk
npm run build --workspace=@gigaedge/denoiser-site
```

Generated WebAssembly assets live under `packages/web-sdk/generated/wasm/` and are not committed.
Regenerate them through the staging scripts instead of editing them manually. `npm pack` validates
the release manifest before packaging.

## Develop the documentation and demo

```bash
cd web
npm start --workspace=@gigaedge/denoiser-site
```

The demo imports the workspace SDK during development and packs the SDK before a production build.
See [`apps/site/README.md`](apps/site/README.md) for testing and GitHub Pages
details.
