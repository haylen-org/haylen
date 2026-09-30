// The app module takes its identity, version, orientation and plugins from the haylen keys of gradle.properties, which make.py writes from app.json, so this file never changes per app.
plugins {
    id("com.android.application")
}

fun haylen(key: String): String = providers.gradleProperty("haylen.$key").get()

// Reads a haylen key that lists id=value entries separated by commas.
fun haylenEntries(key: String): List<Pair<String, String>> = haylen(key).split(",").filter { it.isNotEmpty() }.map { it.substringBefore("=") to it.substringAfter("=") }

android {
    namespace = "dev.haylen.app"
    compileSdk = 37

    defaultConfig {
        applicationId = haylen("identifier")
        minSdk = 27
        targetSdk = 37
        versionCode = haylen("versionCode").toInt()
        versionName = haylen("versionName")
        // The manifests of the plugin modules read their values from placeholders, which make.py writes as haylen.placeholder.<name> keys.
        manifestPlaceholders.putAll(providers.gradlePropertiesPrefixedBy("haylen.placeholder.").get().mapKeys { it.key.removePrefix("haylen.placeholder.") })
        manifestPlaceholders["haylenAppName"] = haylen("name")
        manifestPlaceholders["haylenScreenOrientation"] = haylen("orientation")
        manifestPlaceholders["haylenApplication"] = "android.app.Application"
        manifestPlaceholders["haylenLibrary"] = haylen("library")
    }

    // A C++ app brings the engine in its own library, so the Lua player of the haylen library stays out of its APK.
    if (haylen("library") != "haylen") {
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

// HaylenActivity is a GameActivity and so an AppCompatActivity, whose libraries the app declares at the versions of the haylen library, which links the native side of GameActivity 4.4.2.
dependencies {
    implementation("dev.haylen:haylen:${haylen("engineVersion")}")
    implementation("androidx.games:games-activity:4.4.2")
    implementation("androidx.appcompat:appcompat:1.8.0")
    implementation("androidx.activity:activity:1.13.0")
    implementation("androidx.core:core:1.19.1")
    haylenEntries("plugins").forEach { (id, _) -> implementation(project(":$id")) }
}

// The Gradle plugins that the plugins of the app need, which the root project puts on the build classpath.
haylenEntries("gradlePlugins").forEach { (id, _) -> apply(plugin = id) }

apply(from = "app.gradle")
