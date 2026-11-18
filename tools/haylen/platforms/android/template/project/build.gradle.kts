// The Gradle plugins that the plugins of the app apply to the app module, from the repositories of the engine and of the plugins, whose plugin markers put them on the build classpath like plugins declared with `apply false`. The Kotlin plugin sets the Kotlin that the Android plugin builds Kotlin sources with, newer than the one the Android plugin brings by default.
buildscript {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
{{REPOSITORIES}}
    }
    dependencies {
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.21")
{{GRADLE_PLUGINS}}
    }
}

plugins {
    id("com.android.application") version "9.5.1" apply false
    id("com.android.library") version "9.5.1" apply false
    id("com.android.asset-pack") version "9.5.1" apply false
}
