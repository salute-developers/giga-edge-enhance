# GigaEdge Enhance C++ style guide

GigaEdge Enhance uses **clang-format** with `BasedOnStyle: Google` (see `.clang-format`) for **layout**.

**Naming** follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with these **project-specific differences**:

| Topic | Google | GigaEdge Enhance |
|-------|--------|--------|
| Functions / methods | `PascalCase` | **`snake_case`** |
| Constants (`constexpr`, `const` at namespace scope) | `k` + `PascalCase` | **`snake_case`**, no `k` prefix |
| Types, members, parameters | (same) | `PascalCase` types; `snake_case_` class members; `snake_case` elsewhere |

CI enforces formatting on changed C and C++ lines. The repository still contains legacy formatting,
so tree-wide cleanup belongs in a dedicated change rather than feature pull requests.

## Naming rules

### Types

| Kind | Style | Example |
|------|--------|---------|
| Classes, structs, type aliases | `PascalCase` | `PackedModelReader`, `EchoCancelerConfig` |
| `enum class` enumerators | `snake_case` | `VadType::rnnoise` |
| Template parameters | `PascalCase` or single letter | `T`, `SampleType` |
| Namespaces | `snake_case` | `util::sound`, `enhance` |

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

Use fixed-width integers (`int32_t`, `uint32_t`, and optionally `int16_t` / `uint16_t` from `<cstdint>`) instead of plain `int` / `unsigned` in GigaEdge Enhance code. Exceptions: C API signatures you cannot change (e.g. Speex), and third-party callback types that require `int`. Mark a one-off exception with `// gigaedge-enhance-allow-int` on that line.

`clang-tidy` `google-runtime-int` and cpplint `runtime/int` only flag `short` / `long` / `long long`, not plain `int`. Review plain `int` and `unsigned` uses against this rule when touching legacy code.

Public data members on classes are discouraged; prefer accessors.

Avoid `const` on non-static data members when possible — clang-tidy treats them as `Constant` and may not apply the `member_` suffix rule. When you keep `const` members, still use the `member_` suffix manually (e.g. `b0_`, `win_len_`).

### C API (stable within a major version)

Exported C symbols in `core/include/enhance/c_api.h` keep the existing `GigaEdgeEnhance` + `PascalCase` convention (`GigaEdgeEnhanceDenoiseInit`). **No style-driven renames**. Version 2.0 deliberately introduces the approved product/API rename without legacy aliases; subsequent incompatible changes require a new compatibility plan and major version.

### C++ public API (stable surface)

Headers under `core/include/enhance/` that define **`GEE_API`** types (e.g. `Denoiser`, `AudioProcessor`, and `Sequencer`) are the **supported C++ SDK** in the `giga_edge::enhance` namespace. They use **PascalCase** methods (`ProcessChunk`, `GetFinalChunk`, `ReloadConfig`).

**Do not rename public C++ methods for style** without a compatibility plan. Renaming breaks source compatibility and often **ABI** (exported symbol names on Windows/Linux shared libs).

| Layer | Location | Naming |
|-------|----------|--------|
| **C API** | `c_api.h` | Frozen (`GigaEdgeEnhance*` names) |
| **C++ public API** | `GEE_API` in `core/include/enhance/*.h` | Frozen (`PascalCase` methods) until an explicit API version bump; new *additive* APIs may use `snake_case` if documented |
| **C++ implementation** | `core/src/`, handlers, runners | `snake_case` per this guide |
| **Utilities** | `core/util/` | `snake_case` per this guide unless types leak into public headers |
| **Semi-public headers** | e.g. `packed_model.h` in `include/` | Prefer stable names once released |

## Out of scope

- `out/cache/cpm/**`, `build_*/**` — third-party or generated code; do not reformat or rename.
- Generated build output and vendored source are not style targets.

## Enforcement

| Tool | Role |
|------|------|
| `clang-format` | Layout on changed lines via editor and CI |
| `clang-tidy` `readability-identifier-naming` | Optional focused naming review |

### Configuration

- `.clang-format` — required layout rules.
- `.clang-tidy` — naming options for focused manual review.

### Running checks

Check the C and C++ lines changed relative to the current commit:

```bash
git-clang-format --diff HEAD --extensions c,cc,cpp,h,hpp -- core
```

Naming checks require `clang-tidy` and `compile_commands.json`
(`CMAKE_EXPORT_COMPILE_COMMANDS=ON`). Run them only for the affected target and
review the result against the API-compatibility exceptions above.

## Quick reference

```text
Class:     FeatureExtractor
Method:    process_frame(), set_threshold(), config()
Member:    model_params_, bundle_
Struct:    sample_rate, frame_len
Parameter: input_audio_frame
Constant:  max_frame_ms
C API:     GigaEdgeEnhanceDenoiseInit
```
