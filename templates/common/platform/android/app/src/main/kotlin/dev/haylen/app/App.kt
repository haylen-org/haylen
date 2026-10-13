package dev.haylen.app

import android.app.Application
import dev.haylen.HaylenBridge
import org.json.JSONObject

// The application class of the app, which starts before the first activity. The native platform bridge handlers of the app register here with `HaylenBridge.register` of the `dev.haylen` package, and native code of the app sends events to the app at any time with `HaylenBridge.emit`.
class App : Application() {
    private var greetings = 0

    override fun onCreate() {
        super.onCreate()

        // Answers "app.greet" with a greeting for the name that the app sends, and sends "app.greeted" with the count of greetings so far.
        HaylenBridge.register("app.greet") { params, reply ->
            val name = (params as? JSONObject)?.optString("name").orEmpty()
            greetings += 1
            reply.success(JSONObject().put("message", "Hello, $name, from Kotlin."))
            HaylenBridge.emit("app.greeted", JSONObject().put("count", greetings))
        }
    }
}
