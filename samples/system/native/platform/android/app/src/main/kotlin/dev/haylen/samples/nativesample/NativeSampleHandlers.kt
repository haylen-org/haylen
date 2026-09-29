package dev.haylen.samples.nativesample

import dev.haylen.HaylenBridge
import dev.haylen.HaylenCoroutines
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.delay
import org.json.JSONObject

// Answers the methods of the platform handlers test with suspending Kotlin handlers: a greeting, a typed refusal and a slow call that reports its cancellation.
object NativeSampleHandlers {
    @JvmStatic
    fun register() {
        HaylenCoroutines.register("native_sample.greet") { params ->
            delay(50)
            val name = (params as? JSONObject)?.optString("name").orEmpty()
            mapOf("greeting" to "Hello, $name", "language" to "Kotlin")
        }
        HaylenCoroutines.register("native_sample.refuse") {
            throw HaylenBridge.Failure("Kotlin refused on purpose.", "refused", mapOf("reason" to "requested"))
        }
        HaylenCoroutines.register("native_sample.slow") {
            try {
                awaitCancellation()
            } finally {
                HaylenBridge.emit("native_sample.cancelled", mapOf("language" to "Kotlin"))
            }
        }
    }
}
