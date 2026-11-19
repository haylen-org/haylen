// The test APK of the runner, which instruments itself, with the base class of "support" and the Kotlin tests of the folder that "-PhaylenUiTests" names, and which drives the app that "-PhaylenUiApp" names.
plugins {
    id("com.android.test")
}

val tests = providers.gradleProperty("haylenUiTests").orNull
    ?: error("Pass \"-PhaylenUiTests\" with the folder of the Kotlin UI tests of the app. The command \"haylen test --ui\" passes the folder \"ui-tests/android\" of the app.")
val app = providers.gradleProperty("haylenUiApp").orNull
    ?: error("Pass \"-PhaylenUiApp\" with the identifier of the app that the tests drive. The command \"haylen test --ui\" passes the identifier of the app.")

android {
    namespace = "com.haylen.uitests.test"
    targetProjectPath = ":target"
    experimentalProperties["android.experimental.self-instrumenting"] = true
    compileSdk {
        version = release(37) {
            minorApiLevel = 2
        }
    }

    defaultConfig {
        minSdk = 24
        targetSdk = 37
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        manifestPlaceholders["haylenUiApp"] = app
    }

    sourceSets {
        getByName("main") {
            kotlin.directories += listOf(rootDir.resolve("support").path, tests)
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    implementation("androidx.test:runner:1.7.0")
    implementation("androidx.test.ext:junit:1.3.0")
    implementation("androidx.test.uiautomator:uiautomator:2.4.1")
}
