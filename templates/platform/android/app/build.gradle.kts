// The app module takes its identity, version, orientation, native library and plugins from `haylen/haylen.properties`, and its package, splash resources and native libraries from the folders next to it, which `haylen.py` writes from `app.json` every time it prepares the project.
import java.util.Properties

plugins {
    id("com.android.application")
}

val haylen = Properties().apply { load(providers.fileContents(rootProject.layout.projectDirectory.file("haylen/haylen.properties")).asText.get().reader()) }

// Reads a key that lists `id=value` entries separated by commas.
fun haylenEntries(key: String): List<Pair<String, String>> = haylen.getProperty(key).split(",").filter { it.isNotEmpty() }.map { it.substringBefore("=") to it.substringAfter("=") }

android {
    namespace = "dev.haylen.app"
    compileSdk = 37

    defaultConfig {
        applicationId = haylen.getProperty("identifier")
        minSdk = 27
        targetSdk = 37
        versionCode = haylen.getProperty("versionCode").toInt()
        versionName = haylen.getProperty("versionName")
        // The manifests of the plugin modules read their values from placeholders, which `haylen.py` writes as `placeholder.<name>` keys.
        haylen.stringPropertyNames().filter { it.startsWith("placeholder.") }.forEach { manifestPlaceholders[it.removePrefix("placeholder.")] = haylen.getProperty(it) }
        manifestPlaceholders["haylenAppName"] = haylen.getProperty("name")
        manifestPlaceholders["haylenScreenOrientation"] = haylen.getProperty("orientation")
        manifestPlaceholders["haylenLibrary"] = haylen.getProperty("library")
    }

    sourceSets.getByName("main") {
        assets.directories.add("../haylen/assets")
        res.directories.add("../haylen/res")
        jniLibs.directories.add("../haylen/jniLibs")
    }

    // A C++ app brings the engine in its own library, so the Lua player of the `haylen` library stays out of its APK.
    if (haylen.getProperty("library") != "haylen") {
        packaging {
            jniLibs {
                excludes += "lib/*/libhaylen.so"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

// The `haylen` library brings GameActivity, AppCompat, the activity library and core at the versions it links and builds on, so the app declares none of them.
dependencies {
    implementation("dev.haylen:haylen:${haylen.getProperty("engineVersion")}")
    haylenEntries("plugins").forEach { (id, _) -> implementation(project(":$id")) }
}

// The Gradle plugins that the plugins of the app need, which the root project puts on the build classpath.
haylenEntries("gradlePlugins").forEach { (id, _) -> apply(plugin = id) }
