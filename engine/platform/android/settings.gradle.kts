// Builds the `haylen` Android library, which `make.py engine --platform android` publishes to the Maven repository of the engine artifacts.
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
include(":haylen")
