package dev.haylen.samples.nativesample

import dev.haylen.HaylenBridge
import dev.haylen.HaylenPlugin
import dev.haylen.HaylenPluginContext
import dev.haylen.registerSuspend
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.delay
import org.json.JSONObject

// Native part of the native sample on Android: suspending Kotlin handlers for a greeting, a typed refusal and a slow call that reports its cancellation, and a Java handler that throws.
class NativeSamplePlugin : HaylenPlugin() {
    override fun onLoad(context: HaylenPluginContext) {
        context.registerSuspend("greet") { params ->
            delay(50)
            val name = (params as? JSONObject)?.optString("name").orEmpty()
            mapOf("greeting" to "Hello, $name", "language" to "Kotlin")
        }
        context.registerSuspend("refuse") {
            throw HaylenBridge.Failure("Kotlin refused on purpose.", "refused", mapOf("reason" to "requested"))
        }
        context.registerSuspend("slow") {
            try {
                awaitCancellation()
            } finally {
                context.emit("cancelled", mapOf("language" to "Kotlin"))
            }
        }
        context.register("explode", ExplodeHandler())
    }
}
