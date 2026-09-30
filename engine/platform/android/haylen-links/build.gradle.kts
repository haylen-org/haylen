// Declares `HaylenLinkActivity` of the `haylen` library, which receives the links and notification taps of the app, exported so the plugins that depend on this library add their intent filters to it. An app without such plugins has no exported entry point besides its launcher.
plugins {
    id("com.android.library")
}

android {
    namespace = "dev.haylen.links"
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
