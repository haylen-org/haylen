// Declares `HaylenPluginProvider` of the `haylen` library, which loads the plugins of the app when its process starts. Every plugin module depends on this library, so an app without plugins has no provider.
plugins {
    id("com.android.library")
}

android {
    namespace = "dev.haylen.pluginprovider"
    compileSdk = 37

    defaultConfig {
        minSdk = 27
    }

    publishing {
        singleVariant("release")
    }
}

dependencies {
    api(project(":haylen"))
}
