// Android library module of the plugin `native-sample`. The Android backend copies it into `plugins/native-sample/` of the Android project that it generates for the app, whose app module depends on it. The `haylen-plugins` library brings the plugin API of the `haylen` library and the provider that loads the plugin, and the `haylen-coroutines` library the suspending handlers.
plugins {
    id("com.android.library")
}

android {
    namespace = "com.haylen.plugins.nativesample"
    compileSdk {
        version = release(37) {
            minorApiLevel = 2
        }
    }

    defaultConfig {
        minSdk = 27
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

// The Android project of the app gives every module the engine version it builds with.
val haylenEngineVersion = extra["haylenEngineVersion"] as String

dependencies {
    implementation("dev.haylen:haylen-plugins:$haylenEngineVersion")
    implementation("dev.haylen:haylen-coroutines:$haylenEngineVersion")
}
