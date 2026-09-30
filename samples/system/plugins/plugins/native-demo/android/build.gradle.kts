// Android library module of the Native Demo plugin. The script `make.py` copies it into `plugins/native-demo` of the Android project of an app, which depends on it, and the `haylen` library of the app provides the plugin API at runtime.
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

dependencies {
    compileOnly("dev.haylen:haylen:${providers.gradleProperty("haylen.engineVersion").get()}")
}
