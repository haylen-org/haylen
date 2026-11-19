package com.haylen.plugins.nativebase

// The helper that the plugin `native-base` offers to the plugins that depend on it, whose modules reach this module through the `api` dependency that the Android backend adds to them.
object NativeBase {
    fun describe(text: String): String = "$text, as the native part of native-base wrote it on Android."
}
