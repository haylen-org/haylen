// Registers platform handlers of Kotlin written as suspending functions, for plugins and apps that depend on this library, which brings `kotlinx-coroutines-android` with it.
plugins {
    id("com.android.library")
}

android {
    namespace = "dev.haylen.coroutines"
    compileSdk {
        version = release(37) {
            minorApiLevel = 2
        }
    }

    defaultConfig {
        minSdk = 27
    }

    publishing {
        singleVariant("release")
    }
}

dependencies {
    api(project(":haylen"))
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.11.0")
}
