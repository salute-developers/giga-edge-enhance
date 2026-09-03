---
title: CI and releases
---

# CI and releases

Pull-request CI validates affected source areas without publishing. Release workflows package and
publish only trusted revisions that passed the required platform gates.

## CI principles

- Run the Linux native smoke test before expensive matrices.
- Treat full and DSP WebAssembly compilation as internal stages of the Web target.
- Let each platform target start as soon as its exact engine dependency bundle is available.
- Treat dependency and compiler caches as accelerators, never release sources.
- Upload focused failure diagnostics rather than entire workspaces.
- Use one stable required-gate job so matrix expansion does not change branch protection.
- Do not expose write tokens or release secrets to forked pull requests.

## Release order

`SPENCH_VERSION` is the authoritative version source. Release Please prepares the version and changelog
change. Platform packages are built from the exact tested tag revision, verified, checksummed, and
published before the Web SDK consumes the corresponding WASM artifacts.

A rerun may accept an already published artifact only when its bytes and hash match. It must fail
rather than overwrite an existing version with different content.

## GitHub Pages

The documentation/demo workflows publish the Web artifact that already passed CI. Production and
same-repository previews use separate event-specific workflows, so unrelated skipped jobs do not
appear in pull requests.
