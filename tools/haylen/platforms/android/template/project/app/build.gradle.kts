// The app module takes its identity, version, ABIs, plugins and native libraries from `haylen.json` and the plugins of the app, which the tooling writes into this file every time it prepares the app.
plugins {
    id("com.android.application")
}

// The values of a signing key, each with the suffix of its environment variable.
val keyValues = mapOf("storeFile" to "STORE_FILE", "storePassword" to "STORE_PASSWORD", "keyAlias" to "KEY_ALIAS", "keyPassword" to "KEY_PASSWORD")

// Reads the key of a build type from the environment variables `HAYLEN_<TYPE>_<VALUE>`, which the tooling sets for the build from the key folder of the app or from the key that continuous integration gives, so no password reaches a file of this project or a command line.
fun haylenKey(type: String): Map<String, String>? {
    val given = keyValues.mapValues { (_, variable) -> providers.environmentVariable("HAYLEN_${type.uppercase()}_$variable").orNull }
    if (given.values.any { it == null }) {
        return null
    }
    return given.mapValues { it.value!! }
}

val haylenKeys = listOf("debug", "release").mapNotNull { type -> haylenKey(type)?.let { type to it } }.toMap()

android {
    namespace = {{NAMESPACE}}
    // The NDK that built the native libraries of the app strips their symbols and debug information from the APK, while the build folder keeps them for the debugger and for crash reports.
    ndkVersion = {{NDK_VERSION}}
    compileSdk {
        version = release({{COMPILE_SDK}}) {
            minorApiLevel = 2
        }
    }

    defaultConfig {
        applicationId = {{IDENTIFIER}}
        minSdk = {{MINIMUM_SDK}}
        targetSdk = {{TARGET_SDK}}
        versionCode = {{VERSION_CODE}}
        versionName = {{VERSION_NAME}}
{{PLACEHOLDERS}}
        ndk {
            abiFilters += listOf({{ABIS}})
        }
{{NATIVE_ARGUMENTS}}
    }
{{SOURCE_SETS}}
{{NATIVE_BUILD}}
{{ASSET_PACKS}}

    // The shards of a protected release stay uncompressed in the APK, so the runtime reads their ranges in place.
    androidResources {
        noCompress += "hpak"
    }
{{PACKAGING}}

    // A key of the app replaces the debug key of the Android SDK in debug builds, and the upload key signs release builds.
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
        // A release keeps only the code and the resources that the app reaches, which R8 finds from the manifest, the layouts and the code, unless `platforms.android.minify` of `haylen.json` is false.
        release {
            isMinifyEnabled = {{MINIFY}}
            isShrinkResources = {{MINIFY}}
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
            signingConfig = signingConfigs.findByName("release")
            // The bundle carries the line tables of the native libraries to Google Play, which shows the function, the file and the line of each native frame of a crash.
            ndk {
                debugSymbolLevel = "FULL"
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

// The `haylen` library brings GameActivity, AppCompat, the activity library and core at the versions it links and builds on, so the app declares none of them.
dependencies {
{{DEPENDENCIES}}
}
{{APPLIED_PLUGINS}}
