// The app module takes its identity, version, orientation, native library and plugins from `haylen/haylen.properties`, and its package, splash resources and native libraries from the folders next to it, which `haylen.py` writes from `app.json` every time it prepares the project.
import java.util.Properties

plugins {
    id("com.android.application")
}

val haylen = Properties().apply { load(providers.fileContents(rootProject.layout.projectDirectory.file("haylen/haylen.properties")).asText.get().reader()) }

// Reads a key that lists `id=value` entries separated by commas.
fun haylenEntries(key: String): List<Pair<String, String>> = haylen.getProperty(key).split(",").filter { it.isNotEmpty() }.map { it.substringBefore("=") to it.substringAfter("=") }

// The values of a signing key, each with the suffix of its environment variable.
val keyValues = mapOf("storeFile" to "STORE_FILE", "storePassword" to "STORE_PASSWORD", "keyAlias" to "KEY_ALIAS", "keyPassword" to "KEY_PASSWORD")

// Reads the key of a build type that `haylen.py android-key` writes into `keystore/` with its `<type>.properties`, or else the one that continuous integration gives through the Gradle properties `haylen.<type>.<value>` or the environment variables `HAYLEN_<TYPE>_<VALUE>`, with the keystore as an absolute path.
fun haylenKey(type: String): Map<String, String>? {
    val keystore = rootProject.layout.projectDirectory.dir("keystore")
    val own = providers.fileContents(keystore.file("$type.properties")).asText.orNull
    if (own != null) {
        val values = Properties().apply { load(own.reader()) }
        val key = keyValues.keys.associateWith { values.getProperty(it) ?: throw GradleException("The file keystore/$type.properties has no \"$it\". Create the key again with \"python3 haylen.py android-key\" and the app folder.") }
        return key + ("storeFile" to keystore.file(key.getValue("storeFile")).asFile.path)
    }
    val given = keyValues.mapValues { (name, variable) -> providers.gradleProperty("haylen.$type.$name").orElse(providers.environmentVariable("HAYLEN_${type.uppercase()}_$variable")).orNull }
    if (given.values.any { it == null }) {
        return null
    }
    return given.mapValues { it.value!! } + ("storeFile" to rootProject.file(given.getValue("storeFile")!!).path)
}

val haylenKeys = listOf("debug", "release").mapNotNull { type -> haylenKey(type)?.let { type to it } }.toMap()

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

    // The shards of a protected release stay uncompressed in the APK, so the runtime reads their ranges in place.
    androidResources {
        noCompress += "hpak"
    }

    // A C++ app brings the engine in its own library, so the Lua player of the `haylen` library stays out of its APK.
    if (haylen.getProperty("library") != "haylen") {
        packaging {
            jniLibs {
                excludes += "lib/*/libhaylen.so"
            }
        }
    }

    // A project key replaces the debug key of the Android SDK in debug builds, and the upload key signs release builds.
    signingConfigs {
        haylenKeys.forEach { (type, key) ->
            maybeCreate(type).apply {
                storeFile = file(key.getValue("storeFile"))
                storePassword = key.getValue("storePassword")
                keyAlias = key.getValue("keyAlias")
                keyPassword = key.getValue("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
            signingConfig = signingConfigs.findByName("release")
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

// A release build ships the protected release of the app with a library of its own, while the Lua player of the `haylen` library plays the package of a debug build as it is.
if (haylen.getProperty("library") == "haylen") {
    val development = "The project holds the development package of the app, which release builds never ship. Prepare it with \"python3 haylen.py prepare\" and the app folder with \"--platform android --config Release\"."
    tasks.named { it.startsWith("pre") && it.endsWith("ReleaseBuild") }.configureEach { doFirst { throw GradleException(development) } }
}

// A release build without the upload key stops with the way to add it.
if ("release" !in haylenKeys) {
    val missing = "The release build needs the upload key of the app. Create it with \"python3 haylen.py android-key\" and the app folder, or give the Gradle properties \"haylen.release.storeFile\", \"haylen.release.storePassword\", \"haylen.release.keyAlias\" and \"haylen.release.keyPassword\", or the environment variables \"HAYLEN_RELEASE_STORE_FILE\", \"HAYLEN_RELEASE_STORE_PASSWORD\", \"HAYLEN_RELEASE_KEY_ALIAS\" and \"HAYLEN_RELEASE_KEY_PASSWORD\"."
    tasks.named { it.startsWith("pre") && it.endsWith("ReleaseBuild") }.configureEach { doFirst { throw GradleException(missing) } }
}
