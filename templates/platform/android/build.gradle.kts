// The Gradle plugins that the plugins of the app apply to the app module, which `make.py` lists in `gradlePlugins` of `haylen/haylen.properties` as `id=version` entries. Their plugin markers put them on the build classpath, like plugins declared with `apply false`.
buildscript {
    val haylen = java.util.Properties().apply { load(providers.fileContents(layout.projectDirectory.file("haylen/haylen.properties")).asText.get().reader()) }
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
    dependencies {
        haylen.getProperty("gradlePlugins").split(",").filter { it.isNotEmpty() }.forEach { entry ->
            val (id, version) = entry.split("=")
            classpath("$id:$id.gradle.plugin:$version")
        }
    }
}

plugins {
    id("com.android.application") version "9.4.1" apply false
    id("com.android.library") version "9.4.1" apply false
}
