// The app that the test module of the runner names as its target, without code, which no device installs, since the test APK instruments itself.
plugins {
    id("com.android.application")
}

android {
    namespace = "com.haylen.uitests"
    compileSdk {
        version = release(37) {
            minorApiLevel = 2
        }
    }

    defaultConfig {
        applicationId = "com.haylen.uitests"
        minSdk = 24
        targetSdk = 37
        versionCode = 1
        versionName = "1.0.0"
    }
}
