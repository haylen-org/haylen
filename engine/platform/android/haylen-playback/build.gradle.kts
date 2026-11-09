// Declares `HaylenPlaybackService` of the `haylen` library, which keeps an app that plays audio in the background in the foreground while it plays, with its permissions, so only the apps whose `app.json` sets `audio.background` declare a foreground service.
plugins {
    id("com.android.library")
}

android {
    namespace = "dev.haylen.playback"
    compileSdk {
        version = release(37) {
            minorApiLevel = 2
        }
    }

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
