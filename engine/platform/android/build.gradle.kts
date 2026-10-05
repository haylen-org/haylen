// Every module publishes its release variant as `dev.haylen:<module>` at the engine version to the Maven repository that `haylen.py engine --platform android` names.
plugins {
    id("com.android.library") version "9.4.1" apply false
}

val engineVersion = rootDir.resolve("../../VERSION").readText().trim()
val mavenDir = providers.gradleProperty("haylenMavenDir").orNull
    ?: error("Pass \"-PhaylenMavenDir\" with the Maven repository that receives the libraries. The script \"haylen.py\" uses \"build/artifacts/android/maven\".")

// The Android plugin creates the component of the release variant after the module evaluated, so the publication takes it after the plugin has registered its own step.
subprojects {
    group = "dev.haylen"
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
