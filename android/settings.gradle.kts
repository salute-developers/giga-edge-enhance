pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "giga-denoiser-android-sdk"
include(":giga-denoiser-sdk")
include(":giga-denoiser-model-quality48")
include(":demo-app")

project(":giga-denoiser-sdk").projectDir = file("sdk")
project(":giga-denoiser-model-quality48").projectDir = file("model-quality48")
