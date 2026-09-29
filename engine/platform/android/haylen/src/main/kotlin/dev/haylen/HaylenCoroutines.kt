package dev.haylen

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlin.coroutines.cancellation.CancellationException

// Registers platform handlers written as suspending functions. Each call runs in a coroutine on the main thread, and its result answers the call. A thrown HaylenBridge.Failure fails it with its code and data, any other exception fails it with the code exception, and the coroutine is cancelled when the app cancels the call or its timeout passes.
object HaylenCoroutines {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    @JvmStatic
    fun register(method: String, handler: suspend (params: Any?) -> Any?) {
        HaylenBridge.register(method) { params, reply ->
            val job = scope.launch {
                try {
                    reply.success(handler(params))
                } catch (cancelled: CancellationException) {
                    throw cancelled
                } catch (error: Exception) {
                    reply.failure(error)
                }
            }
            reply.onCancel { job.cancel() }
        }
    }
}
