# syntax=docker/dockerfile:1
# Development-only toolchains. Source and build outputs are mounted at runtime.
FROM ubuntu:24.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ARG NVM_VERSION=v0.40.7

RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        build-essential \
        ca-certificates \
        clang \
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

FROM base AS native

ARG EIGEN_REVISION=e7248b26a1ed53fa030c5c459f7ea095dfd276ac

# ONNX Runtime requires this revision; GitLab's generated ZIP no longer matches
# the historical checksum recorded by ONNX Runtime.
RUN git init /opt/eigen && \
    git -C /opt/eigen remote add origin https://gitlab.com/libeigen/eigen.git && \
    git -C /opt/eigen fetch --depth 1 origin "${EIGEN_REVISION}" && \
    git -C /opt/eigen checkout --detach FETCH_HEAD

FROM base AS android

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

FROM native AS wasm

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
