// Android project of a Haylen app. It has no C++: the player, the activity and the platform bridge come from the `haylen` library that `haylen.py engine --platform android` publishes.
import java.util.Properties

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

// The script `haylen.py` writes `haylen/haylen.properties` from `app.json` and the plugins of the app every time it prepares the project: the engine repository and version, the identity of the app, the native library its activity loads, the plugin modules and their Gradle plugins and manifest placeholders, and the folder that holds the build outputs.
val haylen = Properties().apply { load(providers.fileContents(layout.settingsDirectory.file("haylen/haylen.properties")).asText.get().reader()) }

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        maven {
            name = "haylen"
            url = uri(haylen.getProperty("repository"))
        }
        google()
        mavenCentral()
    }
}

rootProject.name = "app"
include(":app")

// The library modules of the plugins of the app, which `haylen.py` copies into `haylen/plugins/` and lists as `id=folder` entries.
haylen.getProperty("plugins").split(",").filter { it.isNotEmpty() }.forEach { entry ->
    val (id, folder) = entry.split("=")
    include(":$id")
    project(":$id").projectDir = file(folder)
}

// Every module finds the engine version in the extra property `haylenEngineVersion`, which the plugin modules depend on the engine libraries with, and builds into the build folder of the app, outside this project. The action takes plain values, since Gradle keeps it apart from this script.
run {
    val engineVersion = haylen.getProperty("engineVersion")
    val buildDirectory = haylen.getProperty("buildDirectory")
    gradle.lifecycle.beforeProject {
        extra["haylenEngineVersion"] = engineVersion
        layout.buildDirectory.set(file(buildDirectory + path.replace(':', '/')))
    }
}
