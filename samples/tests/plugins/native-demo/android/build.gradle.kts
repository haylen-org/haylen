// Android library module of the Native Demo plugin. The script `haylen.py` copies it into `haylen/plugins/native-demo` of the Android project of an app, which depends on it. The `haylen-plugins` library brings the plugin API of the `haylen` library and the provider that loads the plugin, and the `haylen-links` library the activity that receives its links.
plugins {
    id("com.android.library")
}

android {
    namespace = "dev.haylen.plugins.nativedemo"
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
    implementation("dev.haylen:haylen-links:$haylenEngineVersion")
}
