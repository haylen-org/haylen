// Android library module of the Music Links plugin, whose manifest only adds the intent filter of its scheme to the manifest of the app. The script `haylen.py` copies it into `haylen/plugins/music-links` of the Android project of the app, which depends on it. The `haylen-links` library brings the activity that receives the links.
plugins {
    id("com.android.library")
}

android {
    namespace = "com.haylen.plugins.musiclinks"
    compileSdk {
        version = release(37) {
            minorApiLevel = 2
        }
    }

    defaultConfig {
        minSdk = 27
    }
}

// The Android project of the app gives every module the engine version it builds with.
val haylenEngineVersion = extra["haylenEngineVersion"] as String

dependencies {
    implementation("dev.haylen:haylen-links:$haylenEngineVersion")
}
