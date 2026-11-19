// The runner of the UI tests of apps on Android, which "haylen test --ui <app> --target android" builds with the Kotlin tests of the folder "ui-tests/android" of the app into a test APK that instruments itself and drives the app under test from its own process through UI Automator, as a person does. The Android plugin builds a test module only against an app module, which is "target", an app without code that no device installs.
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

rootProject.name = "haylen-ui-tests"
include(":target", ":runner")
