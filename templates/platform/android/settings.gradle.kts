// Android project of a Haylen app. It has no C++: the player, the activity and the platform bridge come from the haylen library that make.py engine --platform android publishes.
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
        maven {
            name = "haylen"
            url = uri(providers.gradleProperty("haylen.repository").get())
        }
        google()
        mavenCentral()
    }
}

rootProject.name = "app"
include(":app")
