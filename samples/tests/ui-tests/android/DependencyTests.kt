package com.haylen.uitests

import org.junit.Test

// PLG-024: the Kotlin of the plugin "native-demo" calls the helper of the plugin "native-base" that it depends on, whose module the Android backend adds to the module of "native-demo".
class DependencyTests : TestProjectCase() {
    override val code = "PLG-024"

    @Test
    fun theNativePartReachesThePluginItDependsOn() {
        expectCheck("The native part of \"native-demo\" reaches \"native-base\"", containing = "Kotlin called the helper of \"native-base\"")
        expectCheck("The native part of \"native-base\" answers the app", containing = "as the native part of native-base wrote it on Android.")
        saveScreenshot("dependency")
    }
}
