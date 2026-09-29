// Android side of the Haylen runtime: the Lua player library, the activity with its splash screen, the platform bridge and the Kotlin transport of the Varn HTTP client.
// make.py engine --platform android builds the player with the engine CMake project for every ABI and publishes this library as dev.haylen:haylen to a local Maven repository, so apps made from the Android template depend on it without compiling C++.
plugins {
    id("com.android.library")
    `maven-publish`
}

val engineDir = projectDir.resolve("../../..").canonicalFile
val engineVersion = engineDir.resolve("VERSION").readText().trim()
val nativeLibraries = providers.gradleProperty("haylenNativeLibraries").orNull
    ?: error("Pass -PhaylenNativeLibraries with the folder that holds libhaylen.so in a subfolder for each ABI. make.py builds it with the engine CMake project.")
val varnSourceDir = providers.gradleProperty("haylenVarnSourceDir").orNull
    ?: error("Pass -PhaylenVarnSourceDir with the Varn source folder. make.py resolves it from the native build.")
val mavenDir = providers.gradleProperty("haylenMavenDir").orNull
    ?: error("Pass -PhaylenMavenDir with the Maven repository that receives the library. make.py uses build/artifacts/android/maven.")

android {
    namespace = "dev.haylen"
    compileSdk = 37
    // Gradle strips the players with the tools of the NDK that make.py builds them with.
    ndkVersion = "30.0.16248370"

    // miniaudio plays through AAudio from Android 8.1 on, because AAudio of Android 8.0 has known faults, and the engine builds it without OpenSL ES.
    defaultConfig {
        minSdk = 27
        consumerProguardFiles("consumer-rules.pro")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    publishing {
        singleVariant("release")
    }
}

dependencies {
    implementation("androidx.core:core:1.19.1")
    implementation("androidx.core:core-splashscreen:1.2.0")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.11.0")
}

// Only the HTTP transport of the Varn Android library runs inside a Haylen app, because the engine links Varn into the player library itself.
abstract class VarnTransportTask : DefaultTask() {
    @get:InputFile
    abstract val source: RegularFileProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun copy() {
        val target = outputDir.file("com/varn/VarnHttp.kt").get().asFile
        target.parentFile.mkdirs()
        source.get().asFile.copyTo(target, overwrite = true)
    }
}

val varnTransport = tasks.register<VarnTransportTask>("copyVarnTransport") {
    source.set(File(varnSourceDir, "android/varn/src/main/kotlin/com/varn/VarnHttp.kt"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.kotlin?.addGeneratedSourceDirectory(varnTransport, VarnTransportTask::outputDir)
        variant.sources.jniLibs?.addStaticSourceDirectory(nativeLibraries)
    }
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = "dev.haylen"
            artifactId = "haylen"
            version = engineVersion
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
