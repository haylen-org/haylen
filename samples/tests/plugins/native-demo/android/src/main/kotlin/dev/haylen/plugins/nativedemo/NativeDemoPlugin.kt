package dev.haylen.plugins.nativedemo

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Shader
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.provider.OpenableColumns
import android.util.Log
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.app.NotificationManagerCompat
import dev.haylen.HaylenActivity
import dev.haylen.HaylenAudioStream
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPlacement
import dev.haylen.HaylenPlugin
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenRequirements
import dev.haylen.HaylenScreen
import dev.haylen.HaylenVideoStream
import java.io.ByteArrayOutputStream
import java.util.UUID
import org.json.JSONObject

// Native part of the Native Demo plugin on Android, built on the views, dialogs and intents of the platform alone, with the simulations of SDKs in `NativeDemoDevice` and `NativeDemoStore`. The `haylen` library creates it from the meta-data of its manifest and loads it when the app process starts.
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
    private var permissions: ActivityResultLauncher<String>? = null
    private var asking: ((Boolean) -> Unit)? = null
    private lateinit var device: NativeDemoDevice
    private var video: NativeDemoVideo? = null
    private var tone: NativeDemoTone? = null
    private var lastError: JSONObject? = null

    override fun onLoad(context: HaylenPluginContext) {
        this.context = context
        device = NativeDemoDevice(context, ::ask)
        device.register()
        NativeDemoStore(context).register()
        registerCalls()
        registerBytes()
        registerEvents()
        registerStreams()
        registerBanner()
        registerScreens()
        registerPermissions()
        registerRequirements()
        context.emitRetained("loaded", JSONObject().put("language", LANGUAGE).put("platform", "android"))
    }

    private fun registerCalls() {
        context.register("echo") { params, reply ->
            reply.success(JSONObject().put("echo", (params as JSONObject).opt("value")).put("thread", threadName()).put("language", LANGUAGE))
        }

        // Handlers registered with the `BACKGROUND` threading share one background thread of the `haylen` library.
        context.register("compute", { params, reply ->
            val primes = countPrimes((params as JSONObject).getInt("limit"))
            reply.success(JSONObject().put("primes", primes).put("thread", threadName()).put("detail", "the thread \"" + Thread.currentThread().name + "\"").put("language", LANGUAGE))
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

        // Every app that loads the Lua API sends `start`. The plugin ends what an earlier app of the process left running and hands the new app the error that stopped the earlier one.
        context.register("start") { _, reply ->
            stopTicking()
            stopBursts()
            video?.stop()
            video = null
            tone?.stop()
            tone = null
            banner?.remove()
            banner = null
            device.stop()
            lastError?.let { context.emitRetained("lastError", it) }
            lastError = null
            reply.success(null)
        }
    }

    // The bytes of the app arrive as a `ByteArray` in the parameters, and a `ByteArray` in the answer crosses back as bytes.
    private fun registerBytes() {
        context.register("echoBytes") { params, reply ->
            val data = (params as JSONObject).opt("data") as? ByteArray ?: throw HaylenBridge.Failure("The method \"echoBytes\" needs bytes.", "invalidParams", null)
            reply.success(JSONObject().put("data", data).put("size", data.size).put("thread", threadName()).put("language", LANGUAGE))
        }

        context.register("generatedImage", { params, reply ->
            val width = (params as JSONObject).optInt("width")
            val height = params.optInt("height")
            if (width !in 1..2048 || height !in 1..2048) {
                throw HaylenBridge.Failure("The method \"generatedImage\" needs a width and a height from 1 to 2048.", "invalidParams", null)
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

    // Sends `count` batched events 30 times per second for `ticks` ticks, which reach the app as one list per frame, and then `burstDone`.
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

    // The pattern pushes RGBA bitmaps into the video stream `pattern` and the tone pushes floats into the audio stream `tone`, each from a thread of its own, which the app reads through `handle:videoStream` and `handle:audioStream`.
    private fun registerStreams() {
        context.register("startVideo") { _, reply ->
            val stream = context.openVideoStream("pattern", NativeDemoVideo.WIDTH, NativeDemoVideo.HEIGHT, HaylenVideoStream.Format.RGBA8)
            video?.stop()
            video = NativeDemoVideo(stream)
            reply.success(JSONObject().put("width", NativeDemoVideo.WIDTH).put("height", NativeDemoVideo.HEIGHT).put("fps", NativeDemoVideo.FPS).put("format", "RGBA").put("thread", "a HandlerThread").put("language", LANGUAGE))
        }

        context.register("stopVideo") { _, reply ->
            video?.stop()
            video = null
            reply.success(null)
        }

        context.register("startTone") { params, reply ->
            val stream = context.openAudioStream("tone", NativeDemoTone.SAMPLE_RATE, 1, HaylenAudioStream.Format.FLOAT32, NativeDemoTone.SAMPLE_RATE)
            val frequency = (params as? JSONObject)?.optDouble("frequency", 440.0) ?: 440.0
            tone?.stop()
            tone = NativeDemoTone(frequency, stream)
            reply.success(JSONObject().put("frequency", frequency).put("sampleRate", NativeDemoTone.SAMPLE_RATE).put("channels", 1).put("format", "float32").put("language", LANGUAGE))
        }

        context.register("stopTone") { _, reply ->
            tone?.stop()
            tone = null
            reply.success(null)
        }
    }

    private fun registerBanner() {
        context.register("showBanner") { params, reply ->
            val json = params as JSONObject
            val anchor = when (json.getString("anchor")) {
                "top" -> HaylenPlacement.Anchor.TOP
                "bottom" -> HaylenPlacement.Anchor.BOTTOM
                else -> throw HaylenBridge.Failure("The anchor of the banner is \"top\" or \"bottom\", not \"${json.getString("anchor")}\".", "invalidAnchor", null)
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
            val current = banner ?: throw HaylenBridge.Failure("No banner shows. Call \"showBanner\" first.", "noBanner", null)
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

        // The confirm screen of the plugin, an AndroidX activity of its own that the activity of the app starts through the Activity Result API under the key `haylen.native-demo.confirm`. The engine covers the app before the screen shows and until it ends, the Back button ends it with the code `cancelled`, and a screen that the process ended under reaches the next app as `screenRestored`.
        context.registerScreen("confirm", NativeDemoConfirm(), HaylenScreen.Input { params -> (params as JSONObject).put("color", bannerColor()) }, HaylenScreen.Output { confirmed -> JSONObject().put("confirmed", confirmed).put("via", "an AndroidX activity").put("language", LANGUAGE) })

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

    // The permissions go through the Activity Result API after the plugin checked that the manifest of the app declares them, and the notification comes after its seconds through an alarm, whether the app runs, waits in the background or was closed.
    private fun registerPermissions() {
        context.register("requestPermission") { params, reply ->
            val kind = (params as JSONObject).getString("kind")
            val permission = when (kind) {
                "camera" -> Manifest.permission.CAMERA
                "notifications" -> Manifest.permission.POST_NOTIFICATIONS
                else -> throw HaylenBridge.Failure("The permission is \"camera\" or \"notifications\", not \"$kind\".", "invalidPermission", null)
            }
            if (kind == "camera" && !context.application().packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) {
                throw HaylenBridge.Failure("This device has no camera.", "unsupported", null)
            }
            // Android 12 and earlier ask for no permission to post notifications, which the person turns off in the settings instead.
            if (kind == "notifications" && Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                reply.success(permissionAnswer(kind, NotificationManagerCompat.from(context.application()).areNotificationsEnabled()))
                return@register
            }
            context.requirements().require(HaylenRequirements.Requirement.permission(permission))
            ask(permission) { granted -> reply.success(permissionAnswer(kind, granted)) }
        }

        context.register("notify") { params, reply ->
            val json = params as JSONObject
            context.requirements().require(HaylenRequirements.Requirement.permission(Manifest.permission.POST_NOTIFICATIONS))
            if (!NotificationManagerCompat.from(context.application()).areNotificationsEnabled()) {
                throw HaylenBridge.Failure("The person has not allowed the notifications of the app. Ask with \"requestPermission('notifications')\" first.", "permissionDenied", null)
            }
            val identifier = NOTIFICATION_PREFIX + UUID.randomUUID()
            val seconds = json.getDouble("seconds")
            NativeDemoNotifier.schedule(context.application(), identifier, json.getString("title"), json.getString("body"), seconds)
            reply.success(JSONObject().put("identifier", identifier).put("seconds", seconds).put("language", LANGUAGE))
        }
    }

    // Asks the person for a permission through the launcher of the activity and calls `answered` with whether the person granted it. One request shows at a time.
    private fun ask(permission: String, answered: (Boolean) -> Unit) {
        val launcher = permissions ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
        if (asking != null) {
            throw HaylenBridge.Failure("Another permission request shows.", "busy", null)
        }
        asking = answered
        launcher.launch(permission)
    }

    // A request that ends after the process ended reaches the launcher of the new activity, while no call of the new app waits for it.
    private fun onPermission(granted: Boolean) {
        val answered = asking ?: run {
            Log.i(TAG, "The permission request answered ${if (granted) "granted" else "not granted"} after the app started again, and no call waits for it.")
            return
        }
        asking = null
        answered(granted)
    }

    private fun permissionAnswer(kind: String, granted: Boolean): JSONObject =
        JSONObject().put("kind", kind).put("granted", granted).put("status", if (granted) "authorized" else "denied").put("language", LANGUAGE)

    // The plugin needs the permission to read the contacts, which its manifest leaves out on purpose, so the call shows how a requirement that the project of the app lacks fails with the code `unsupported` and lists what is missing in `data.missing`. An app that declares the permission gets the answer.
    private fun registerRequirements() {
        context.register("requirementCheck") { _, reply ->
            context.requirements().require(HaylenRequirements.Requirement.permission(Manifest.permission.READ_CONTACTS))
            reply.success(JSONObject().put("met", true).put("language", LANGUAGE))
        }
    }

    // A pick that ends after the process ended reaches the launcher of the new activity, while no call of the new app waits for it.
    private fun onPicked(uri: Uri?) {
        val reply = picking
        picking = null
        if (reply == null) {
            Log.i(TAG, "The document picker answered ${if (uri == null) "nothing" else "\"$uri\""} after the app started again, and no call waits for it.")
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
        permissions = activity.activityResultRegistry.register(PERMISSION_KEY, activity, ActivityResultContracts.RequestPermission(), ::onPermission)
        if (savedInstanceState == null) {
            openUrl(activity.intent)
            openNotification(activity.intent)
        }
    }

    override fun onNewIntent(intent: Intent) {
        openUrl(intent)
        openNotification(intent)
    }

    // The tap on a notification of the plugin reaches the app as `notificationOpened`, retained, so the tap that launched the app waits for the first listener.
    private fun openNotification(intent: Intent?) {
        if (intent?.action == NativeDemoNotifier.ACTION_OPENED) {
            context.emitRetained("notificationOpened", JSONObject().put("identifier", intent.getStringExtra(NativeDemoNotifier.IDENTIFIER)).put("title", intent.getStringExtra(NativeDemoNotifier.TITLE)).put("action", "open").put("language", LANGUAGE))
        }
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
        device.activityDestroyed()
        picker = null
        permissions = null
    }

    // The error screen of the app shows this error. The plugin keeps it and hands it to the next app when that app sends `start`.
    override fun onAppError(error: JSONObject) {
        lastError = JSONObject().put("message", error.optString("message")).put("file", error.optString("file")).put("line", error.optInt("line")).put("language", LANGUAGE)
    }

    private fun bannerColor(): Int {
        val text = context.config().getString("bannerColor")
        if (!COLOR.matches(text)) {
            throw HaylenBridge.Failure("The parameter \"bannerColor\" must be a color as \"#RRGGBB\", not \"$text\".", "invalidColor", null)
        }
        return Color.parseColor(text)
    }

    private companion object {
        const val LANGUAGE = "Kotlin"
        const val TAG = "native-demo"
        const val PICK_KEY = "native-demo.pickFile"
        const val PERMISSION_KEY = "native-demo.requestPermission"

        // The notifications of the plugin carry this prefix in their identifiers, as on Apple platforms.
        const val NOTIFICATION_PREFIX = "native-demo."
        val COLOR = Regex("#[0-9A-Fa-f]{6}")

        fun threadName(): String = if (Looper.myLooper() == Looper.getMainLooper()) "main" else "background"

        // Draws the pattern of the demo, a gradient from red to green with blue stripes, into a `Bitmap` and compresses it as a PNG file.
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
