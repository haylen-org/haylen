// Android side of the Haylen runtime: the Lua player library, the activity with its splash screen, the platform bridge, the plugin API and the Kotlin transport of the Varn HTTP client. Its manifest declares only what every app needs, so permissions and the components of plugins and links come from the app and from the libraries that need them.
// The command `haylen.py engine --platform android` builds the player with the engine CMake project for every ABI and publishes this library as `dev.haylen:haylen` to a local Maven repository, so apps made from the Android template depend on it without compiling C++.
plugins {
    id("com.android.library")
}

val nativeLibraries = providers.gradleProperty("haylenNativeLibraries").orNull
    ?: error("Pass \"-PhaylenNativeLibraries\" with the folder that holds \"libhaylen.so\" in a subfolder for each ABI. The script \"haylen.py\" builds it with the engine CMake project.")
val varnSourceDir = providers.gradleProperty("haylenVarnSourceDir").orNull
    ?: error("Pass \"-PhaylenVarnSourceDir\" with the Varn source folder. The script \"haylen.py\" resolves it from the native build.")

android {
    namespace = "dev.haylen"
    compileSdk = 37
    // Gradle strips the players with the tools of the NDK that `haylen.py` builds them with.
    ndkVersion = "30.0.16248370"

    // The miniaudio library plays through AAudio from Android 8.1 on, because AAudio of Android 8.0 has known faults, and the engine builds it without OpenSL ES.
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

// The class `HaylenActivity` extends `GameActivity`, whose native side the engine links into `libhaylen.so` from the AAR of the same version, so no other version of its Java classes may reach an app. The class `GameActivity` extends `AppCompatActivity`, and its POM declares no dependencies, so AppCompat, the activity library and core come from here.
dependencies {
    api("androidx.games:games-activity") {
        version {
            strictly("4.4.2")
        }
    }
    api("androidx.appcompat:appcompat:1.8.0")
    api("androidx.activity:activity:1.13.0")
    api("androidx.core:core:1.19.1")
    implementation("androidx.core:core-splashscreen:1.2.0")
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
