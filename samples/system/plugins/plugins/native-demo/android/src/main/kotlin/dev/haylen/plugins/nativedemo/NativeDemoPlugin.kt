package dev.haylen.plugins.nativedemo

import android.content.ActivityNotFoundException
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Shader
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.provider.OpenableColumns
import android.util.Log
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import dev.haylen.HaylenActivity
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPlacement
import dev.haylen.HaylenPlugin
import dev.haylen.HaylenPluginContext
import java.io.ByteArrayOutputStream
import org.json.JSONObject

// Native part of the Native Demo plugin on Android, built on the views, dialogs and intents of the platform alone. The haylen library creates it from the meta-data of its manifest and loads it when the app process starts.
class NativeDemoPlugin : HaylenPlugin() {
    private lateinit var context: HaylenPluginContext
    private val mainThread = Handler(Looper.getMainLooper())
    private var ticker: Runnable? = null
    private var ticks = 0
    private var bursts: Runnable? = null
    private var banner: NativeDemoBanner? = null
    private var bannerState = JSONObject()
    private var bannerTaps = 0
    private var picker: ActivityResultLauncher<Array<String>>? = null
    private var picking: HaylenBridge.Reply? = null
    private var lastError: JSONObject? = null

    override fun onLoad(context: HaylenPluginContext) {
        this.context = context
        registerCalls()
        registerBytes()
        registerEvents()
        registerStreams()
        registerBanner()
        registerScreens()
        context.emitRetained("loaded", JSONObject().put("language", LANGUAGE).put("platform", "android"))
    }

    private fun registerCalls() {
        context.register("echo") { params, reply ->
            reply.success(JSONObject().put("echo", (params as JSONObject).opt("value")).put("thread", threadName()).put("language", LANGUAGE))
        }

        // Handlers registered with the BACKGROUND threading share one background thread of the haylen library.
        context.register("compute", { params, reply ->
            val primes = countPrimes((params as JSONObject).getInt("limit"))
            reply.success(JSONObject().put("primes", primes).put("thread", threadName()).put("detail", "the thread " + Thread.currentThread().name).put("language", LANGUAGE))
        }, HaylenBridge.Threading.BACKGROUND)

        context.register("fail") { _, reply ->
            reply.failure("The native demo failed on purpose.", "demoFailure", JSONObject().put("reason", "requested").put("language", LANGUAGE))
        }

        // The call never answers by itself, so only a cancel or a timeout of the app ends it, and the plugin tells the app that it heard it.
        context.register("wait") { params, reply ->
            val token = (params as JSONObject).getInt("token")
            reply.onCancel { context.emit("waitCancelled", JSONObject().put("token", token).put("language", LANGUAGE)) }
        }

        context.register("config") { _, reply -> reply.success(context.config()) }

        // Every app that loads the Lua API sends start. The plugin ends what an earlier app of the process left running and hands the new app the error that stopped the earlier one.
        context.register("start") { _, reply ->
            stopTicking()
            stopBursts()
            banner?.remove()
            banner = null
            lastError?.let { context.emitRetained("lastError", it) }
            lastError = null
            reply.success(null)
        }
    }

    // The bytes of the app arrive as a ByteArray in the parameters, and a ByteArray in the answer crosses back as bytes.
    private fun registerBytes() {
        context.register("echoBytes") { params, reply ->
            val data = (params as JSONObject).opt("data") as? ByteArray ?: throw HaylenBridge.Failure("echoBytes needs bytes.", "invalidParams", null)
            reply.success(JSONObject().put("data", data).put("size", data.size).put("thread", threadName()).put("language", LANGUAGE))
        }

        context.register("generatedImage", { params, reply ->
            val width = (params as JSONObject).optInt("width")
            val height = params.optInt("height")
            if (width !in 1..2048 || height !in 1..2048) {
                throw HaylenBridge.Failure("generatedImage needs a width and a height from 1 to 2048.", "invalidParams", null)
            }
            reply.success(JSONObject().put("png", drawPattern(width, height)).put("width", width).put("height", height).put("drawnWith", "an Android Bitmap and Canvas").put("language", LANGUAGE))
        }, HaylenBridge.Threading.BACKGROUND)
    }

    private fun registerEvents() {
        context.register("burst") { params, reply ->
            val count = (params as JSONObject).getInt("count")
            val ticks = params.getInt("ticks")
            startBursts(count, ticks)
            reply.success(JSONObject().put("count", count).put("ticks", ticks).put("language", LANGUAGE))
        }

        context.register("ticks") { params, reply ->
            val enabled = (params as JSONObject).getBoolean("enabled")
            val interval = params.getDouble("interval")
            stopTicking()
            if (enabled) {
                ticks = 0
                val delay = (interval * 1000).toLong()
                val tick = object : Runnable {
                    override fun run() {
                        ticks += 1
                        context.emit("tick", JSONObject().put("count", ticks).put("thread", threadName()).put("language", LANGUAGE))
                        mainThread.postDelayed(this, delay)
                    }
                }
                ticker = tick
                mainThread.postDelayed(tick, delay)
            }
            reply.success(JSONObject().put("enabled", enabled).put("interval", interval))
        }
    }

    private fun stopTicking() {
        ticker?.let { mainThread.removeCallbacks(it) }
        ticker = null
    }

    // Sends count batched events 30 times per second for ticks ticks, which reach the app as one list per frame, and then burstDone.
    private fun startBursts(count: Int, ticks: Int) {
        stopBursts()
        var tick = 0
        val burst = object : Runnable {
            override fun run() {
                for (index in 0 until count) {
                    context.emit("burst", JSONObject().put("tick", tick).put("index", index).put("language", LANGUAGE), false, true)
                }
                tick += 1
                if (tick < ticks) {
                    mainThread.postDelayed(this, 1000L / 30)
                } else {
                    bursts = null
                    context.emit("burstDone", JSONObject().put("events", count * ticks).put("ticks", ticks).put("language", LANGUAGE))
                }
            }
        }
        bursts = burst
        mainThread.post(burst)
    }

    private fun stopBursts() {
        bursts?.let { mainThread.removeCallbacks(it) }
        bursts = null
    }

    // Video and audio streams from Kotlin need the stream API of the Android library, which a later version of the engine brings, so the plugin says so instead.
    private fun registerStreams() {
        for (method in listOf("startVideo", "stopVideo", "startTone", "stopTone")) {
            context.register(method) { _, reply ->
                reply.failure("The Android library of this engine has no stream API yet, so Kotlin cannot push video frames or audio samples to the app.", "unsupported", null)
            }
        }
    }

    private fun registerBanner() {
        context.register("showBanner") { params, reply ->
            val json = params as JSONObject
            val anchor = when (json.getString("anchor")) {
                "top" -> HaylenPlacement.Anchor.TOP
                "bottom" -> HaylenPlacement.Anchor.BOTTOM
                else -> throw HaylenBridge.Failure("The anchor of the banner is top or bottom, not ${json.getString("anchor")}.", "invalidAnchor", null)
            }
            val placement = HaylenPlacement(anchor)
            placement.reserve = json.getBoolean("reserve")
            placement.widthDp = NativeDemoBanner.WIDTH_DP
            placement.heightDp = NativeDemoBanner.HEIGHT_DP
            val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            val current = banner
            if (current == null) {
                banner = NativeDemoBanner(activity, context.config().getString("greeting"), bannerColor(), context.overlay(), placement) {
                    bannerTaps += 1
                    context.emit("bannerTapped", JSONObject().put("count", bannerTaps).put("language", LANGUAGE))
                }
            } else {
                current.place(placement)
            }
            bannerState = JSONObject().put("anchor", json.getString("anchor")).put("reserve", placement.reserve).put("visible", banner!!.isVisible)
            reply.success(bannerState)
        }

        context.register("setBannerVisible") { params, reply ->
            val current = banner ?: throw HaylenBridge.Failure("No banner shows. Call showBanner first.", "noBanner", null)
            current.isVisible = (params as JSONObject).getBoolean("visible")
            bannerState.put("visible", current.isVisible)
            reply.success(bannerState)
        }

        context.register("removeBanner") { _, reply ->
            banner?.remove()
            banner = null
            reply.success(null)
        }
    }

    // The native screen covers the app while it shows, so the app stands still and stays silent until Close ends the cover. The document picker is an activity of its own, which pauses the app the usual way and answers through the Activity Result API.
    private fun registerScreens() {
        context.register("showScreen") { params, reply ->
            val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            val title = (params as JSONObject).optString("title", "Native screen")
            val color = bannerColor()
            val started = SystemClock.elapsedRealtime()
            context.coverApp()
            NativeDemoScreen(activity, title, color) {
                context.uncoverApp()
                reply.success(JSONObject().put("seconds", (SystemClock.elapsedRealtime() - started) / 1000.0))
            }.show()
        }

        context.register("pickFile") { _, reply ->
            val launcher = picker ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            try {
                launcher.launch(arrayOf("*/*"))
                picking = reply
            } catch (missing: ActivityNotFoundException) {
                reply.failure("No app on this device picks documents.", "unsupported", null)
            }
        }
    }

    // A pick that ends after the process ended reaches the launcher of the new activity, while no call of the new app waits for it.
    private fun onPicked(uri: Uri?) {
        val reply = picking
        picking = null
        if (reply == null) {
            Log.i(TAG, "The document picker answered ${uri ?: "nothing"} after the app started again, and no call waits for it.")
            return
        }
        if (uri == null) {
            reply.success(null)
            return
        }
        val name = displayName(uri)
        if (name == null) {
            reply.failure("The picked document has no name.", "noName", null)
        } else {
            reply.success(JSONObject().put("name", name))
        }
    }

    private fun displayName(uri: Uri): String? {
        val activity = context.activity() ?: return null
        return activity.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) cursor.getString(0) else null
        }
    }

    // The activity takes the launcher of the document picker before it starts, under a key that stays the same for every activity, so the result that Android delivers again after the end of the process finds it.
    // A link that launches the app arrives with the intent of the new activity, and one that reaches the running app arrives as a new intent. The retained event waits for the first listener either way.
    override fun onActivityCreated(activity: HaylenActivity, savedInstanceState: Bundle?) {
        picker = activity.activityResultRegistry.register(PICK_KEY, activity, ActivityResultContracts.OpenDocument(), ::onPicked)
        if (savedInstanceState == null) {
            openUrl(activity.intent)
        }
    }

    override fun onNewIntent(intent: Intent) {
        openUrl(intent)
    }

    private fun openUrl(intent: Intent?) {
        val url = intent?.data ?: return
        if (intent.action == Intent.ACTION_VIEW && url.scheme == context.config().getString("urlScheme")) {
            context.emitRetained("urlOpened", JSONObject().put("url", url.toString()))
        }
    }

    // The activity takes the views of the overlay and its launchers with it, so a new activity starts without a banner.
    override fun onActivityDestroyed(activity: HaylenActivity) {
        banner = null
        picker = null
    }

    // The error screen of the app shows this error. The plugin keeps it and hands it to the next app when that app sends start.
    override fun onAppError(error: JSONObject) {
        lastError = JSONObject().put("message", error.optString("message")).put("file", error.optString("file")).put("line", error.optInt("line")).put("language", LANGUAGE)
    }

    private fun bannerColor(): Int {
        val text = context.config().getString("bannerColor")
        if (!COLOR.matches(text)) {
            throw HaylenBridge.Failure("The bannerColor parameter must be a color as #RRGGBB, not $text.", "invalidColor", null)
        }
        return Color.parseColor(text)
    }

    private companion object {
        const val LANGUAGE = "Kotlin"
        const val TAG = "native-demo"
        const val PICK_KEY = "native-demo.pickFile"
        val COLOR = Regex("#[0-9A-Fa-f]{6}")

        fun threadName(): String = if (Looper.myLooper() == Looper.getMainLooper()) "main" else "background"

        // Draws the pattern of the demo, a gradient from red to green with blue stripes, into a Bitmap and compresses it as a PNG file.
        fun drawPattern(width: Int, height: Int): ByteArray {
            val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bitmap)
            canvas.drawPaint(Paint().apply { shader = LinearGradient(0f, 0f, width.toFloat(), height.toFloat(), Color.RED, Color.GREEN, Shader.TileMode.CLAMP) })
            val stripe = Paint().apply { color = Color.argb(153, 0, 0, 230) }
            var offset = -height.toFloat()
            while (offset < width) {
                canvas.drawPath(Path().apply {
                    moveTo(offset, 0f)
                    lineTo(offset + 16f, 0f)
                    lineTo(offset + 16f + height, height.toFloat())
                    lineTo(offset + height, height.toFloat())
                    close()
                }, stripe)
                offset += 32f
            }
            val png = ByteArrayOutputStream()
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, png)
            bitmap.recycle()
            return png.toByteArray()
        }

        fun countPrimes(limit: Int): Int {
            if (limit <= 2) {
                return 0
            }
            val composite = BooleanArray(limit)
            var count = 0
            for (number in 2 until limit) {
                if (composite[number]) {
                    continue
                }
                count += 1
                var multiple = number.toLong() * number
                while (multiple < limit) {
                    composite[multiple.toInt()] = true
                    multiple += number
                }
            }
            return count
        }
    }
}
