// The Kotlin that the Android plugin builds Kotlin sources with, newer than the one the plugin brings by default.
buildscript {
    repositories {
        mavenCentral()
    }
    dependencies {
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.21")
    }
}

// Every module publishes its release variant as `dev.haylen:<module>` at the engine version to the Maven repository that `haylen sdk build --target android` names, and builds into the folder that it names, outside the engine sources.
plugins {
    id("com.android.library") version "9.5.1" apply false
}

val engineVersion = rootDir.resolve("../../VERSION").readText().trim()
val mavenDir = providers.gradleProperty("haylenMavenDir").orNull
    ?: error("Pass \"-PhaylenMavenDir\" with the Maven repository that receives the libraries. The command \"haylen sdk build --target android\" passes the folder \"maven\" of the SDK.")
val buildDirectory = providers.gradleProperty("haylenBuildDirectory").orNull
    ?: error("Pass \"-PhaylenBuildDirectory\" with the folder that receives the build outputs. The command \"haylen sdk build --target android\" passes a folder of \"build/engine\".")

// The Android plugin creates the component of the release variant after the module evaluated, so the publication takes it after the plugin has registered its own step.
subprojects {
    group = "dev.haylen"
    layout.buildDirectory.set(file("$buildDirectory/$name"))
    version = engineVersion
    apply(plugin = "maven-publish")
    pluginManager.withPlugin("com.android.library") {
        configure<PublishingExtension> {
            publications {
                register<MavenPublication>("release") {
                    afterEvaluate {
                        from(components["release"])
                    }
                }
            }
            repositories {
                maven {
                    name = "artifacts"
                    url = uri(mavenDir)
                }
            }
        }
    }
}
