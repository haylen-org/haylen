// Android library module of the {{TITLE}} plugin. The script `make.py` copies it into `plugins/{{ID}}` of the Android project of an app, which depends on it. The `haylen-plugins` library brings the plugin API of the `haylen` library and the provider that loads the plugin when the app process starts.
plugins {
    id("com.android.library")
}

android {
    namespace = "dev.haylen.plugins.{{PACKAGE}}"
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
    implementation("dev.haylen:haylen-plugins:${providers.gradleProperty("haylen.engineVersion").get()}")
}
