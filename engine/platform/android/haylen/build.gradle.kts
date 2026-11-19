// Android side of the Haylen runtime: the Lua player library, the activity with its splash screen, the platform bridge, the plugin API, the Kotlin transport of the Varn HTTP client and the Java class of Varn data views. Its manifest declares only what every app needs, so permissions and the components of plugins and links come from the app and from the libraries that need them.
// The command `haylen sdk build --target android` builds the player with the engine CMake project for every ABI of the SDK and publishes this library as `dev.haylen:haylen` to the local Maven repository of the SDK, so the generated projects of Lua apps depend on it without compiling C++.
plugins {
    id("com.android.library")
}

val nativeLibraries = providers.gradleProperty("haylenNativeLibraries").orNull
    ?: error("Pass \"-PhaylenNativeLibraries\" with the folder that holds \"libhaylen.so\" in a subfolder for each ABI. The command \"haylen sdk build --target android\" builds it with the engine CMake project.")
val varnSourceDir = providers.gradleProperty("haylenVarnSourceDir").orNull
    ?: error("Pass \"-PhaylenVarnSourceDir\" with the Varn source folder. The command \"haylen sdk build --target android\" resolves it from the native build.")

android {
    namespace = "dev.haylen"
    compileSdk {
        version = release(37) {
            minorApiLevel = 2
        }
    }
    // Gradle strips the players with the tools of the NDK that the SDK builds them with.
    ndkVersion = "30.0.16248370"

    // The miniaudio library plays through AAudio from Android 8.1 on, because AAudio of Android 8.0 has known faults, and the engine builds it without OpenSL ES.
    defaultConfig {
        minSdk = 27
        consumerProguardFiles("consumer-rules.pro")
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
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
    implementation("androidx.customview:customview:1.2.0")
    implementation("androidx.window:window-java:1.5.1")

    // The tests of the library: the logic without Android on the JVM, and the classes that need Android on a device or the emulator.
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
}

// Only the HTTP transport and the data views of the Varn Android library run inside a Haylen app, because the engine links Varn into the player library itself and registers their natives from it.
abstract class VarnSourceTask : DefaultTask() {
    @get:InputFile
    abstract val source: RegularFileProperty

    @get:Input
    abstract val destination: Property<String>

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun copy() {
        val target = outputDir.file(destination.get()).get().asFile
        target.parentFile.mkdirs()
        source.get().asFile.copyTo(target, overwrite = true)
    }
}

val varnTransport = tasks.register<VarnSourceTask>("copyVarnTransport") {
    source.set(File(varnSourceDir, "android/varn/src/main/kotlin/com/varn/VarnHttp.kt"))
    destination.set("com/varn/VarnHttp.kt")
}

val varnDataView = tasks.register<VarnSourceTask>("copyVarnDataView") {
    source.set(File(varnSourceDir, "android/varn/src/main/java/com/varn/VarnDataView.java"))
    destination.set("com/varn/VarnDataView.java")
}

androidComponents {
    onVariants { variant ->
        variant.sources.kotlin?.addGeneratedSourceDirectory(varnTransport, VarnSourceTask::outputDir)
        variant.sources.java?.addGeneratedSourceDirectory(varnDataView, VarnSourceTask::outputDir)
        variant.sources.jniLibs?.addStaticSourceDirectory(nativeLibraries)
    }
}
