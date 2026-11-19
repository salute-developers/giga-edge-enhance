plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.library) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.detekt) apply false
}

// VERSION_NUMBER at the repository root is the authoritative release version for every GigaEdgeEnhance
// SDK, so the Android modules read it instead of duplicating the literal. CI overrides it with
// GEE_ARTIFACT_VERSION when an artifact name needs a SemVer prerelease suffix, matching the
// behaviour of cmake/Install.cmake.
val gigaEdgeEnhanceVersion: String = System.getenv("GEE_ARTIFACT_VERSION")
    ?.takeIf(String::isNotBlank)
    ?: rootDir.resolve("../VERSION_NUMBER").normalize().readText().trim()

allprojects {
    version = gigaEdgeEnhanceVersion
}
