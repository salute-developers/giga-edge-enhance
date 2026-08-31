# syntax=docker/dockerfile:1@sha256:ecfaec9ed6d810b56388c508f4121597bfbba70d41a6dfeee4d8cad5f295fc32
# Development-only toolchains. Source and build outputs are mounted at runtime.
FROM ubuntu:24.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ARG NVM_VERSION=v0.40.7

RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        build-essential \
        ca-certificates \
        ccache \
        clang \
        clang-tidy \
        cpplint \
        libclang-rt-18-dev \
        cmake \
        curl \
        git \
        ninja-build \
        openjdk-17-jdk-headless \
        pkg-config \
        python3 \
        unzip \
        llvm \
        zip && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /usr/local/lib/nvm && \
    /bin/bash -o pipefail -c 'curl --fail --location --retry 3 \
        "https://raw.githubusercontent.com/nvm-sh/nvm/${NVM_VERSION}/install.sh" | \
        NVM_DIR=/usr/local/lib/nvm PROFILE=/dev/null bash'

FROM base AS native-base

ARG EIGEN_REVISION=e7248b26a1ed53fa030c5c459f7ea095dfd276ac

# ONNX Runtime requires this revision; GitLab's generated ZIP no longer matches
# the historical checksum recorded by ONNX Runtime.
RUN git init /opt/eigen && \
    git -C /opt/eigen remote add origin https://gitlab.com/libeigen/eigen.git && \
    git -C /opt/eigen fetch --depth 1 origin "${EIGEN_REVISION}" && \
    git -C /opt/eigen checkout --detach FETCH_HEAD

FROM native-base AS native

FROM base AS android-base

ARG ANDROID_COMMAND_LINE_TOOLS=11076708
ARG ANDROID_NDK_VERSION=29.0.14206865
ARG ANDROID_CMAKE_VERSION=3.22.1

ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_SDK_ROOT=/opt/android-sdk

RUN mkdir -p "${ANDROID_HOME}/cmdline-tools" && \
    curl --fail --location --retry 3 \
        --output /tmp/android-command-line-tools.zip \
        "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_COMMAND_LINE_TOOLS}_latest.zip" && \
    unzip -q /tmp/android-command-line-tools.zip -d "${ANDROID_HOME}/cmdline-tools" && \
    mv "${ANDROID_HOME}/cmdline-tools/cmdline-tools" "${ANDROID_HOME}/cmdline-tools/latest" && \
    rm /tmp/android-command-line-tools.zip

ENV PATH=${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${PATH}

RUN yes | sdkmanager --licenses && \
    sdkmanager \
        "platform-tools" \
        "platforms;android-35" \
        "build-tools;35.0.0" \
        "cmake;${ANDROID_CMAKE_VERSION}" \
        "ndk;${ANDROID_NDK_VERSION}"

FROM android-base AS android

FROM native-base AS wasm-base

ARG EMSDK_VERSION=3.1.64

RUN git clone --depth 1 --branch "${EMSDK_VERSION}" \
        https://github.com/emscripten-core/emsdk.git /opt/emsdk && \
    /opt/emsdk/emsdk install "${EMSDK_VERSION}" && \
    /opt/emsdk/emsdk activate "${EMSDK_VERSION}" && \
    /bin/bash -c 'source /opt/emsdk/emsdk_env.sh && \
        cd "$EMSDK/upstream/emscripten" && \
        PATH="$(dirname "$EMSDK_NODE"):$PATH" npm ci'

ENV EMSDK=/opt/emsdk
ENV PATH=/opt/emsdk:/opt/emsdk/upstream/emscripten:${PATH}

FROM wasm-base AS wasm

# OCI artifact stages. They intentionally contain dependency bundles only,
# allowing GHCR to store them separately from runnable toolchain images.
FROM scratch AS deps-native
LABEL org.gigaedge.spench.kind="dependency-bundle" \
      org.gigaedge.spench.target="linux-x86_64"
COPY --from=spench_deps /linux/x86_64 /dependencies/linux/x86_64

FROM scratch AS deps-wasm
LABEL org.gigaedge.spench.kind="dependency-bundle" \
      org.gigaedge.spench.target="emscripten-wasm32"
COPY --from=spench_deps /emscripten/wasm32 /dependencies/emscripten/wasm32

FROM scratch AS deps-android
LABEL org.gigaedge.spench.kind="dependency-bundle" \
      org.gigaedge.spench.target="android"
COPY --from=spench_deps /android /dependencies/android

FROM scratch AS deps-ios
LABEL org.gigaedge.spench.kind="dependency-bundle" \
      org.gigaedge.spench.target="ios"
COPY --from=spench_deps /ios /dependencies/ios

FROM scratch AS deps-macos-x86_64
LABEL org.gigaedge.spench.kind="dependency-bundle" \
      org.gigaedge.spench.target="macos-x86_64"
COPY --from=spench_deps /macos/x86_64 /dependencies/macos/x86_64

FROM scratch AS deps-macos-arm64
LABEL org.gigaedge.spench.kind="dependency-bundle" \
      org.gigaedge.spench.target="macos-arm64"
COPY --from=spench_deps /macos/arm64 /dependencies/macos/arm64

FROM scratch AS deps-windows-x64
LABEL org.gigaedge.spench.kind="dependency-bundle" \
      org.gigaedge.spench.target="windows-x86_64"
COPY --from=spench_deps /windows/x86_64 /dependencies/windows/x86_64

FROM base AS web-test

ENV MOS_VENV_DIR=/opt/spench-mos-venv
ENV NISQA_WEIGHTS_HOME=/opt/spench-nisqa-home
ENV NUMBA_CACHE_DIR=/tmp/numba-cache
ENV PLAYWRIGHT_BROWSERS_PATH=/opt/ms-playwright
ENV PATH=/opt/spench-mos-venv/bin:${PATH}

COPY platform_wrappers/web/.nvmrc /tmp/spench-web/.nvmrc
COPY platform_wrappers/web/package.json platform_wrappers/web/package-lock.json /tmp/spench-web/
COPY platform_wrappers/web/packages/web-sdk/package.json /tmp/spench-web/packages/web-sdk/package.json
COPY platform_wrappers/web/services/spench-demo/package.json /tmp/spench-web/services/spench-demo/package.json
COPY platform_wrappers/web/services/spench-demo/mos_install.sh \
    platform_wrappers/web/services/spench-demo/requirements-mos.txt /tmp/mos/

RUN apt-get update && \
    apt-get install --no-install-recommends -y ffmpeg python3-venv && \
    rm -rf /var/lib/apt/lists/* && \
    MOS_VENV_DIR="${MOS_VENV_DIR}" /tmp/mos/mos_install.sh

RUN /bin/bash -c 'source /usr/local/lib/nvm/nvm.sh && \
        nvm install "$(cat /tmp/spench-web/.nvmrc)" && \
        cd /tmp/spench-web && npm ci --ignore-scripts && \
        PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH}" \
        npx playwright install --with-deps chromium'

RUN HOME="${NISQA_WEIGHTS_HOME}" python3 - <<'PY'
import torch
from torchmetrics.functional.audio.nisqa import non_intrusive_speech_quality_assessment

# TorchMetrics downloads the NISQA checkpoint on its first invocation.
try:
    non_intrusive_speech_quality_assessment(torch.zeros(1, 160_000), 16_000)
except RuntimeError:
    pass
PY

RUN mkdir -p "${NUMBA_CACHE_DIR}" && \
    chmod 1777 "${NUMBA_CACHE_DIR}" && \
    rm -rf /root/.cache /root/.npm /tmp/spench-web/node_modules
