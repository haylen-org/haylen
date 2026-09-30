// Android library module of the native sample. The script `make.py` copies it into `haylen/plugins/native-sample` of the Android project of the app, which depends on it. The `haylen-plugins` library brings the plugin API of the `haylen` library and the provider that loads the plugin, and the `haylen-coroutines` library the suspending handlers.
plugins {
    id("com.android.library")
}

android {
    namespace = "dev.haylen.samples.nativesample"
    compileSdk = 37

    defaultConfig {
        minSdk = 27
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

// The Android project of the app gives every module the engine version it builds with.
val haylenEngineVersion: String by extra

dependencies {
    implementation("dev.haylen:haylen-plugins:$haylenEngineVersion")
    implementation("dev.haylen:haylen-coroutines:$haylenEngineVersion")
}
