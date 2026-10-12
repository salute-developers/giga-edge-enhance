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

rootProject.name = "gigaedge-spench-android-sdk"
include(":gigaedge-spench-sdk")
include(":gigaedge-spench-model-quality48")
include(":demo-app")

project(":gigaedge-spench-sdk").projectDir = file("sdk")
project(":gigaedge-spench-model-quality48").projectDir = file("model-quality48")
