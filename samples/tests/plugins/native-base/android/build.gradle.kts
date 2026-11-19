// Android library module of the plugin `native-base`. The Android backend copies it into `plugins/native-base/` of the Android project that it generates for the app, whose app module depends on it, and adds it as an `api` dependency to the modules of the plugins that depend on the plugin, so their code reaches `NativeBase`. The `haylen-plugins` library brings the plugin API of the `haylen` library and the provider that loads the plugin.
plugins {
    id("com.android.library")
}

android {
    namespace = "com.haylen.plugins.nativebase"
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
}
