plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.library) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.detekt) apply false
}

// SPENCH_VERSION at the repository root is the authoritative release version for every Spench
// SDK, so the Android modules read it instead of duplicating the literal. CI overrides it with
// SPENCH_ARTIFACT_VERSION when an artifact name needs a SemVer prerelease suffix, matching the
// behaviour of cmake/SpenchInstall.cmake.
val spenchVersion: String = System.getenv("SPENCH_ARTIFACT_VERSION")
    ?.takeIf(String::isNotBlank)
    ?: rootDir.resolve("../SPENCH_VERSION").normalize().readText().trim()

allprojects {
    version = spenchVersion
}
