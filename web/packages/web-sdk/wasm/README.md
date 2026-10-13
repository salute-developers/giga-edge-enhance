# GigaEdge Spench WebAssembly

## Model packing

Place the inference model at the path referenced by its configuration file, then
pack the configuration and model with the native `model_packer` executable:

```shell
<build-dir>/core/cli/model_packer/model_packer \
  --pack path/to/config.yaml path/to/output/model.ges
```

Build `<build-dir>` with the `native-model-packer` preset. Put the resulting
`.ges` file in the web SDK's `models/` directory or distribute it with the
application.

## Builds

Use the repository [build instructions](../../../../README.md#build-the-repository) for supported
WASM workflows:

```shell
tools/test wasm-release
tools/test wasm-dsp-release
tools/build web
```

## Emscripten

`tools` is the supported WASM workflow. Its Docker image provides the
pinned Emscripten SDK version and requires no host Emscripten installation.

For a manual host build or IDE configuration, install the same pinned SDK
outside the repository, activate it in the shell that runs CMake, and then use
the WASM presets:

```shell
git clone --depth 1 --branch 3.1.64 https://github.com/emscripten-core/emsdk.git ~/opt/emsdk
~/opt/emsdk/emsdk install 3.1.64
~/opt/emsdk/emsdk activate 3.1.64
source ~/opt/emsdk/emsdk_env.sh

cmake --preset wasm-release
cmake --build --preset wasm-release
```

The presets read the `EMSDK` environment variable to locate Emscripten's CMake
toolchain. Do not install Emscripten under ONNX Runtime's dependency directory:
that directory is managed by the build and can be recreated at any time.

For IDE navigation, configure the appropriate WASM preset after activating the
SDK. CMake writes `compile_commands.json` to the preset build directory.
