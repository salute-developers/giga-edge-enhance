---
title: CI and releases
---

# CI and releases

Pull-request CI validates affected source areas without publishing. Release workflows package and
publish only trusted revisions that passed the required platform gates.

## CI principles

- Run the Linux native smoke test before expensive matrices.
- Keep native, WebAssembly/web, Android, macOS/iOS, and Windows jobs independent after smoke passes.
- Treat dependency and compiler caches as accelerators, never release sources.
- Upload focused failure diagnostics rather than entire workspaces.
- Use one stable required-gate job so matrix expansion does not change branch protection.
- Do not expose write tokens or release secrets to forked pull requests.

## Release order

`VERSION` is the authoritative version source. Release Please prepares the version and changelog
change. Platform packages are built from the exact tested tag revision, verified, checksummed, and
published before the Web SDK consumes the corresponding WASM artifacts.

A rerun may accept an already published artifact only when its bytes and hash match. It must fail
rather than overwrite an existing version with different content.

## GitHub Pages

The documentation/demo workflow builds one static Docusaurus output. Production and same-repository
preview builds supply different base paths but otherwise run the same website build.
