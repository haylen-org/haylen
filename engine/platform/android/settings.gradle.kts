// Builds the Haylen Android libraries, which `haylen.py engine --platform android` publishes to the Maven repository of the engine artifacts. The library `haylen` holds the player, the activity and the plugin API, and each of the others brings one thing that only some apps want: `haylen-plugins` the provider that loads plugins, `haylen-links` the activity that receives links and notifications, and `haylen-coroutines` the suspending handlers of Kotlin.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "haylen-android"
include(":haylen", ":haylen-plugins", ":haylen-links", ":haylen-coroutines")
