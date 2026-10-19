// Android library module of the sign-in of Tiny Island. The script `haylen.py` copies it into `haylen/plugins/sign-in` of the Android project of the game, which depends on it. The `haylen-plugins` library brings the plugin API of the `haylen` library and the provider that loads the plugin, and Credential Manager the sign-in.
plugins {
    id("com.android.library")
}

android {
    namespace = "com.haylen.plugins.signin"
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
    implementation("androidx.credentials:credentials:1.6.0")
    implementation("androidx.credentials:credentials-play-services-auth:1.6.0")
    implementation("com.google.android.libraries.identity.googleid:googleid:1.2.1")
}
