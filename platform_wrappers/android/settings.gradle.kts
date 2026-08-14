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

rootProject.name = "spench-android-sdk"
include(":spench-sdk")
include(":spench-model-full24k")
include(":spench-model-full48k")
include(":demo-app")
