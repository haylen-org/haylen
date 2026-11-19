// The Kotlin that the Android plugin builds Kotlin sources with, newer than the one the plugin brings by default.
buildscript {
    repositories {
        mavenCentral()
    }
    dependencies {
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.21")
    }
}

// The runner builds into the folder that the tooling names, outside the engine sources, with the reports of Gradle.
plugins {
    id("com.android.application") version "9.5.0" apply false
    id("com.android.test") version "9.5.0" apply false
}

val buildDirectory = providers.gradleProperty("haylenBuildDirectory").orNull
    ?: error("Pass \"-PhaylenBuildDirectory\" with the folder that receives the build outputs. The command \"haylen test --ui\" passes a folder of \"build/engine\".")

layout.buildDirectory.set(file(buildDirectory))
subprojects {
    layout.buildDirectory.set(file("$buildDirectory/$name"))
}
