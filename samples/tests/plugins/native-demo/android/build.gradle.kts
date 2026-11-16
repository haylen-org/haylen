// Android library module of the Native Demo plugin. The Android backend copies it into `plugins/native-demo/` of the Android project that it generates for an app, whose app module depends on it. The `haylen-plugins` library brings the plugin API of the `haylen` library and the provider that loads the plugin, and the `haylen-links` library the activity that receives its links.
plugins {
    id("com.android.library")
}

android {
    namespace = "com.haylen.plugins.nativedemo"
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
    implementation("dev.haylen:haylen-links:$haylenEngineVersion")
}
