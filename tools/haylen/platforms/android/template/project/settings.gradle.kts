// The Gradle project of a Haylen app, which the tooling generates from `haylen.json` and the plugins of the app every time it prepares the app, so nothing in it is edited by hand. The player, the activity and the platform bridge come from the Android libraries of the engine SDK.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

// The Maven repositories of the SDKs of the plugins come after the repository of the engine SDK.
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        maven {
            name = "haylen"
            url = uri({{ENGINE_REPOSITORY}})
        }
        google()
        mavenCentral()
{{REPOSITORIES}}
    }
}

rootProject.name = "app"
include(":app")
{{MODULES}}

// Every module finds the engine version in the extra property `haylenEngineVersion`, which the plugin modules depend on the engine libraries with, and builds into the build folder of the app, outside this project. The action takes plain values, since Gradle keeps it apart from this script.
run {
    val engineVersion = {{ENGINE_VERSION}}
    val buildDirectory = {{BUILD_DIRECTORY}}
    gradle.lifecycle.beforeProject {
        extra["haylenEngineVersion"] = engineVersion
        layout.buildDirectory.set(file(buildDirectory + path.replace(':', '/')))
    }
}
