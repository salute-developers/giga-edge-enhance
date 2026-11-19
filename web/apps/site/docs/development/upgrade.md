# Upgrade to GigaEdge Enhance 2.0

Version 2.0 deliberately changes product identifiers and breaks source and binary compatibility. Rebuild the native libraries and all SDK consumers together. No legacy aliases or exports are provided.

| Surface | New identifier |
| --- | --- |
| C++ | `gigaedge::enhance::Denoiser`, `AudioProcessor`, `EchoCanceller`, `Context`, `Sequencer` |
| Headers | `<enhance.h>`, `<enhance/c_api.h>`, `<enhance/denoiser.h>` |
| C API | `GigaEdgeEnhanceDenoiseInit`, `GigaEdgeEnhanceRetCode` and the `GigaEdgeEnhance*` family |
| CMake | `find_package(gigaedge-enhance CONFIG REQUIRED)`; link `gigaedge::enhance` |
| Native library | `libgigaedge-enhance` |
| Build flags / environment | `GEE_*` |
| Android | `com.gigaedge.enhance`; `Processor`, `Settings`, `Features` |
| Swift | `EnhanceAudioKit`; `EnhanceProcessor`, `EnhanceSettings`, `EnhanceAVAudioUnit` |
| Audio Unit | component subtype `enha`, manufacturer `GgEd` |
| Web | `@gigaedge/enhance-web-sdk`; `GigaEdgeEnhanceSDK.create` |
| Model resources | `enhance/models/<id>.enhance` on Android; `.enhance` files on iOS/Web |

Update include paths, C/JNI symbols, Kotlin imports, R8 rules, Swift imports, bundle metadata, worker/worklet names, npm imports, CMake links and deployment environment variables together. Demo application IDs and persistence keys also change. CI cache and dependency-artifact identities use the new prefix and must be regenerated through the existing tools.

Model IDs, YAML parameters, error-code values, network payloads and the packed file layout are unchanged. Renaming a model resource does not convert its contents. Keep using `fast8`, `quality48` and `hybrid48` according to each platform catalogue. Dependency versions and upstream repositories remain pinned by `package-lock.cmake`; the new branding does not introduce private dependencies.
