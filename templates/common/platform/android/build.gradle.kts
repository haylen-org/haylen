// The Gradle plugins that the plugins of the app apply to the app module, which `haylen.py` lists in `gradlePlugins` of `haylen/haylen.properties` as `id=version` entries, from the repositories of the engine and of the plugins. Their plugin markers put them on the build classpath, like plugins declared with `apply false`. The Kotlin plugin sets the Kotlin that the Android plugin builds Kotlin sources with, newer than the one the Android plugin brings by default.
buildscript {
    val haylen = java.util.Properties().apply { load(providers.fileContents(layout.projectDirectory.file("haylen/haylen.properties")).asText.get().reader()) }
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
        haylen.getProperty("repositories").split(",").filter { it.isNotEmpty() }.forEach { address ->
            maven { url = uri(address) }
        }
    }
    dependencies {
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.21")
        haylen.getProperty("gradlePlugins").split(",").filter { it.isNotEmpty() }.forEach { entry ->
            val (id, version) = entry.split("=")
            classpath("$id:$id.gradle.plugin:$version")
        }
    }
}

plugins {
    id("com.android.application") version "9.5.0" apply false
    id("com.android.library") version "9.5.0" apply false
    id("com.android.asset-pack") version "9.5.0" apply false
}
