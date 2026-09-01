---
sidebar_position: 1
slug: /
title: Spench documentation
description: Build real-time speech enhancement into native, mobile, and web applications.
---

# Spench documentation

Spench is a cross-platform speech-enhancement SDK for noise suppression, loudness normalization,
and acoustic echo cancellation. Choose a platform guide or start with the interactive browser demo.

## Choose your platform

- [Core C and C++ SDK](/docs/core/) for native applications and direct buffer processing.
- [Web SDK](/docs/web-sdk/) for Web Audio, AudioWorklet, and WebAssembly integration.
- [Android SDK](/docs/android/) for Kotlin/JNI applications.
- [iOS SDK](/docs/ios/) for AVAudioEngine and direct Swift buffer processing.

## Before you begin

Models are distributed separately from the source tree. Repository contributors retrieve the test
models through DVC; SDK consumers should use the model package documented for their platform.
