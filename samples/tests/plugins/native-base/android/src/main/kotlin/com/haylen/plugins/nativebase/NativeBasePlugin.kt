package com.haylen.plugins.nativebase

import dev.haylen.HaylenPlugin
import dev.haylen.HaylenPluginContext
import org.json.JSONObject

// Native part of the plugin `native-base` on Android, whose method answers the app with the text of its helper `NativeBase`.
class NativeBasePlugin : HaylenPlugin() {
    override fun onLoad(context: HaylenPluginContext) {
        context.register("describe") { params, reply ->
            val text = NativeBase.describe((params as JSONObject).getString("text"))
            reply.success(JSONObject().put("text", text).put("language", "Kotlin"))
        }
    }
}
