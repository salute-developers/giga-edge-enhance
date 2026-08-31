plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.detekt)
}

android {
    namespace = "com.gigaedge.spench"
    compileSdk = 35
    ndkVersion = "29.0.14206865"

    defaultConfig {
        val spenchVersion = System.getenv("SPENCH_ARTIFACT_VERSION")
            ?: rootProject.file("../../VERSION").readText().trim()
        versionCode = 1
        versionName = spenchVersion
        minSdk = 26

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20")
                abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64", "x86")
                targets += listOf("spench-sdk")
                System.getenv("SPENCH_PREBUILT_DEPENDENCIES_ROOT")?.let {
                    arguments += "-DSPENCH_PREBUILT_DEPENDENCIES_ROOT=$it"
                }
            }
        }

        consumerProguardFiles("consumer-rules.pro")

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    testOptions {
        unitTests.isIncludeAndroidResources = true
    }
}

detekt {
    buildUponDefaultConfig = true
    config.setFrom(rootProject.file("config/detekt/detekt.yml"))
}

dependencies {
    implementation(libs.androidx.annotation)

    testImplementation(libs.junit)

    androidTestImplementation(libs.androidx.test.ext.junit)
    androidTestImplementation(libs.androidx.test.runner)
    androidTestImplementation(libs.androidx.test.rules)
}
