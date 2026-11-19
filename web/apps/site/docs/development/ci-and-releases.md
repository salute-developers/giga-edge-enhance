---
title: CI and releases
---

# CI and releases

Pull-request CI validates every required source area. Same-repository pull requests also publish the
already-built Web site as a temporary GitHub Pages preview; release workflows publish only trusted
revisions that passed the required platform gates.

## CI principles

- Treat full and DSP WebAssembly compilation as internal stages of the Web target.
- Build each toolchain before its dependency OCI, then start each platform target as soon as its exact
  dependency bundle is available.
- Treat dependency and compiler caches as accelerators, never release sources.
- Upload focused failure diagnostics rather than entire workspaces.
- Keep static Release, sanitizer, Web Debug, and Android Debug emulator coverage in the manual
  validation workflow; regular CI runs Android Release emulator tests.
- Do not expose write tokens or release secrets to forked pull requests.

## Release order

`VERSION_NUMBER` is the authoritative version source. Release Please prepares the version and changelog
change. Platform packages are built from the exact tested tag revision, verified, checksummed, and
published before the Web SDK consumes the corresponding WASM artifacts.

A rerun may accept an already published artifact only when its bytes and hash match. It must fail
rather than overwrite an existing version with different content.

## GitHub Pages

The default-branch Pages workflow publishes the Web artifact under `/enhance/`. Pull-request CI
publishes the artifact immediately after the Web build and unit tests under `/enhance/pr/<number>/`;
Playwright remains an independent check and does not delay the preview. The preview-cleanup workflow
removes that directory when the pull request closes.
