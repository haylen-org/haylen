package com.haylen.plugins.{{PACKAGE}}

import dev.haylen.HaylenPlugin
import dev.haylen.HaylenPluginContext
import org.json.JSONObject
{{BLOCK:ANDROID_IMPORTS}}

// Native part of the {{TITLE}} plugin on Android. The `haylen` library creates it from the `meta-data` of its manifest, and loads it when the app process starts.
class {{NAME}}Plugin : HaylenPlugin() {
    {{BLOCK:ANDROID_MEMBERS}}
    override fun onLoad(context: HaylenPluginContext) {
        // Answers `{{ID}}.echo` with the message it receives, and sends it to the app again as the `{{ID}}.echoed` event.
        context.register("echo") { params, reply ->
            val message = (params as JSONObject).getString("message")
            context.emit("echoed", JSONObject().put("message", message))
            reply.success(JSONObject().put("message", message))
        }
        {{BLOCK:ANDROID_LOAD}}
    }
    {{BLOCK:ANDROID_HOOKS}}
}
