// The app module takes its identity, version and orientation from the haylen keys of gradle.properties, which make.py writes from app.json, so this file never changes per app.
plugins {
    id("com.android.application")
}

fun haylen(key: String): String = providers.gradleProperty("haylen.$key").get()

android {
    namespace = "dev.haylen.app"
    compileSdk = 37

    defaultConfig {
        applicationId = haylen("identifier")
        minSdk = 27
        targetSdk = 37
        versionCode = haylen("versionCode").toInt()
        versionName = haylen("versionName")
        manifestPlaceholders["haylenAppName"] = haylen("name")
        manifestPlaceholders["haylenScreenOrientation"] = haylen("orientation")
        manifestPlaceholders["haylenApplication"] = "android.app.Application"
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

dependencies {
    implementation("dev.haylen:haylen:${haylen("engineVersion")}")
}

apply(from = "app.gradle")
