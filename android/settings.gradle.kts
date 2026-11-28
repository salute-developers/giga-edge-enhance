pluginManagement {
    repositories {
        google()
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
    }
}

rootProject.name = "gigaedge-enhance-android-sdk"
include(":gigaedge-enhance-sdk")
include(":gigaedge-enhance-model-quality48")
include(":gigaedge-enhance-model-hybrid48")
include(":demo-app")

project(":gigaedge-enhance-sdk").projectDir = file("sdk")
project(":gigaedge-enhance-model-quality48").projectDir = file("model-quality48")
project(":gigaedge-enhance-model-hybrid48").projectDir = file("model-hybrid48")
