// Android side of the Haylen runtime: the Lua player library built from the engine CMake project, the activity with its splash screen, the platform bridge and the Kotlin transport of the Varn HTTP client.
// make.py engine --platform android publishes it as dev.haylen:haylen to a local Maven repository, so apps made from the Android template depend on it without compiling C++.
plugins {
    id("com.android.library")
    `maven-publish`
}

val engineDir = projectDir.resolve("../../..").canonicalFile
val engineVersion = engineDir.resolve("VERSION").readText().trim()
val sokolShdc = providers.gradleProperty("haylenSokolShdc").orNull
    ?: error("Pass -PhaylenSokolShdc with the sokol-shdc executable. make.py installs it into .tools.")
val varnSourceDir = providers.gradleProperty("haylenVarnSourceDir").orNull
    ?: error("Pass -PhaylenVarnSourceDir with the Varn source folder. make.py resolves it from the native build.")
val mavenDir = providers.gradleProperty("haylenMavenDir").orNull
    ?: error("Pass -PhaylenMavenDir with the Maven repository that receives the library. make.py uses build/artifacts/android/maven.")

android {
    namespace = "dev.haylen"
    compileSdk = 37
    ndkVersion = "30.0.16248370"

    // miniaudio plays through AAudio from Android 8.1 on, because AAudio of Android 8.0 has known faults, and the engine builds it without OpenSL ES.
    defaultConfig {
        minSdk = 27
        consumerProguardFiles("consumer-rules.pro")

        // 32-bit ARM keeps the Android TV devices that still run it, and x86_64 serves emulators.
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf("-DHAYLEN_SOKOL_SHDC=$sokolShdc", "-DHAYLEN_BUILD_PLAYER=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF", "-DANDROID_STL=c++_static", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
                targets += "haylen"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = engineDir.resolve("CMakeLists.txt")
            version = "4.1.2"
        }
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
