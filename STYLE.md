# Spench C++ style guide

Spench uses **clang-format** with `BasedOnStyle: Google` (see `.clang-format`) for **layout**.

**Naming** follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with these **project-specific differences**:

| Topic | Google | Spench |
|-------|--------|--------|
| Functions / methods | `PascalCase` | **`snake_case`** |
| Constants (`constexpr`, `const` at namespace scope) | `k` + `PascalCase` | **`snake_case`**, no `k` prefix |
| Types, members, parameters | (same) | `PascalCase` types; `snake_case_` class members; `snake_case` elsewhere |

Formatting is enforced tree-wide. Naming is enforced on paths listed in `container_internals/style_naming_enforced.txt` (see [Enforcement](#enforcement)).

## Naming rules

### Types

| Kind | Style | Example |
|------|--------|---------|
| Classes, structs, type aliases | `PascalCase` | `PackedModelReader`, `EchoCancelerConfig` |
| `enum class` enumerators | `snake_case` | `VadType::rnnoise` |
| Template parameters | `PascalCase` or single letter | `T`, `SampleType` |
| Namespaces | `snake_case` | `util::sound`, `spench` |

### Functions and methods

| Kind | Style | Example |
|------|--------|---------|
| Functions and methods | `snake_case` | `process_frame()`, `parse_packed_data()` |
| Predicates | `is_` / `has_` prefix | `is_model_bundled()` |
| Simple getters | Noun phrase, no `get_` | `config()`, `model_path()` |
| Non-trivial getters | `get_` prefix | `get_stat_info()`, `get_future_context_len()` |
| Setters / fluent builders | `set_` prefix | `set_sample_rate()` |

Do **not** use `PascalCase` methods (`ProcessFrame`) or `camelCase` methods (`getModel`) in internal code.

### Variables

| Kind | Style | Example |
|------|--------|---------|
| Locals and parameters | `snake_case` | `input_audio_frame`, `sample_rate` |
| Constants (`constexpr`, file-scope `const`) | `snake_case` | `max_frame_ms`, `default_sample_rate` |
| Class data members (private/protected) | `snake_case` + trailing `_` | `config_`, `fft_mutex_` |
| Struct / POD data members | `snake_case`, no suffix | `sample_rate`, `prev_input_chunk` |

Do **not** use the `m_` prefix for members; use `foo_` instead.

Do **not** use the `k` prefix for constants.

Use fixed-width integers (`int32_t`, `uint32_t`, and optionally `int16_t` / `uint16_t` from `<cstdint>`) instead of plain `int` / `unsigned` in Spench code. Exceptions: C API signatures you cannot change (e.g. Speex), and third-party callback types that require `int`. Mark a one-off exception with `// spench-allow-int` on that line.

`clang-tidy` `google-runtime-int` and cpplint `runtime/int` only flag `short` / `long` / `long long`, not plain `int`. Spench uses `container_internals/check_fixed_width_int.sh` for the `int` / `unsigned` rule on the same paths as naming checks.

Public data members on classes are discouraged; prefer accessors.

Avoid `const` on non-static data members when possible — clang-tidy treats them as `Constant` and may not apply the `member_` suffix rule. When you keep `const` members, still use the `member_` suffix manually (e.g. `b0_`, `win_len_`).

### C API (stable, do not rename)

Exported C symbols in `spench/include/spench/spench_c.h` keep the existing `Spench` + `PascalCase` convention (`SpenchDenoiseInit`). **No style-driven renames** — breaking the C API requires a major version and explicit deprecation.

### C++ public API (stable surface)

Headers under `spench/include/spench/` that define **`SPENCH_API`** types (e.g. `SpenchDenoise`, `SpenchAudioProcess`, `ISpenchSequencer`) are the **supported C++ SDK**. They use **PascalCase** methods (`ProcessChunk`, `GetFinalChunk`, `ReloadConfig`).

**Do not rename public C++ methods for style** without a compatibility plan. Renaming breaks source compatibility and often **ABI** (exported symbol names on Windows/Linux shared libs).

| Layer | Location | Naming |
|-------|----------|--------|
| **C API** | `spench_c.h` | Frozen (`Spench*` names) |
| **C++ public API** | `SPENCH_API` in `spench/include/spench/*.h` | Frozen (`PascalCase` methods) until an explicit API version bump; new *additive* APIs may use `snake_case` if documented |
| **C++ implementation** | `spench/src/`, handlers, runners | `snake_case` per this guide |
| **Utilities** | `spench/util/` | `snake_case` per this guide unless types leak into public headers |
| **Semi-public headers** | e.g. `packed_model.h` in `include/` | Prefer stable names once released |

## Out of scope

- `spench/contrib/**`, `spench/submodules/**`, `build_*/**` — third-party code; do not reformat or rename.
- C demos excluded from cpplint: `spench/demo/2-c_denoiser_api_demo.c`, `spench/demo/utils_c.c` (see `container_internals/check_style.sh`).

## Enforcement

| Tool | Role |
|------|------|
| `clang-format` | Layout (whole tree, via editor / CI) |
| `cpplint` | Google-ish lint (`check_style.sh`) |
| `clang-tidy` `readability-identifier-naming` | Naming on **enforced paths only** |

### Configuration

- `.clang-tidy` — naming check options (Spench deltas above)
- `container_internals/style_naming_enforced.txt` — paths where naming is checked (one path per line; directories or individual files)
- `container_internals/run_clang_tidy_naming.sh` — naming checks on `.cpp`, `.cc`, `.h`, and `.hpp` under enforced paths

### Running checks

Default (CI / container): cpplint only.

```bash
./container_internals/check_style.sh
```

Manual mode on the host: cpplint, clang-tidy naming, and fixed-width integer scan.

```bash
./container_internals/check_style.sh -m
```

Fixed-width integers only (fast, no `clang-tidy` required):

```bash
./container_internals/check_fixed_width_int.sh
```

Naming checks require `clang-tidy` and `compile_commands.json` (`CMAKE_EXPORT_COMPILE_COMMANDS=ON`). Set `SPENCH_BUILD_DIR` if needed (e.g. `build_onnx_pocketfft`).

If the project was built in Docker (paths under `/spench/...`), `run_clang_tidy_naming.sh` rewrites those paths to your checkout root when you run checks on the host. CI (`check_style.sh` without `-m`) runs **cpplint** and **fixed-width int** checks. **clang-tidy** naming runs only with `check_style.sh -m` (install `clang-tools-extra` locally, or use an image that provides `clang-tidy`).

To enforce naming in a new directory, add its path to `style_naming_enforced.txt` and run `check_style.sh -m`.

## Quick reference

```text
Class:     FeatureExtractor
Method:    process_frame(), set_threshold(), config()
Member:    model_params_, bundle_
Struct:    sample_rate, frame_len
Parameter: input_audio_frame
Constant:  max_frame_ms
C API:     SpenchDenoiseInit
```
