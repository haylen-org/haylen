package dev.haylen.plugins.nativedemo

import android.Manifest
import android.annotation.SuppressLint
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.ImageFormat
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.params.OutputConfiguration
import android.hardware.camera2.params.SessionConfiguration
import android.location.Location
import android.location.LocationManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.ImageReader
import android.media.MediaRecorder
import android.net.Uri
import android.os.Build
import android.os.CancellationSignal
import android.os.Handler
import android.os.HandlerThread
import android.provider.Settings
import androidx.core.content.ContextCompat
import androidx.core.location.LocationManagerCompat
import dev.haylen.HaylenAudioStream
import dev.haylen.HaylenBridge
import dev.haylen.HaylenOverlay
import dev.haylen.HaylenPlacement
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenRequirements
import dev.haylen.HaylenVideoStream
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.sqrt
import org.json.JSONObject

// The device simulations of the demo on Android, which stand in for the SDKs of cameras, maps and sharing with the APIs of the platform alone: the camera of Camera2 into the video stream `camera` with photos as JPEG bytes, the microphone of `AudioRecord` into the audio stream `microphone` with its level, the location of `LocationManager`, a drawn map over the app, the share sheet and other apps through their intents. The permissions go through the launcher of the plugin, which `ask` reaches, after the plugin checked that the manifest of the app declares them.
class NativeDemoDevice(private val context: HaylenPluginContext, private val ask: (String, (Boolean) -> Unit) -> Unit) {
    private var camera: Camera? = null
    private var microphone: Microphone? = null
    private var map: HaylenOverlay.Panel? = null

    fun register() {
        registerCamera()
        registerMicrophone()
        registerLocation()
        registerSharing()
    }

    // Ends what an earlier app of the process left running.
    fun stop() {
        camera?.stop()
        camera = null
        microphone?.stop()
        microphone = null
        map?.remove()
        map = null
    }

    // The activity takes the views of the overlay with it.
    fun activityDestroyed() {
        map = null
    }

    private fun registerCamera() {
        context.register("startCamera") { params, reply ->
            val facing = (params as? JSONObject)?.optString("facing", "back") ?: "back"
            if (!context.application().packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) {
                throw HaylenBridge.Failure("This device has no camera.", "unsupported", null)
            }
            withPermission(Manifest.permission.CAMERA, reply) {
                val stream = context.openVideoStream("camera", Camera.WIDTH, Camera.HEIGHT, HaylenVideoStream.Format.RGBA8)
                camera?.stop()
                camera = Camera(context.application(), facing, stream) { failure ->
                    if (failure == null) {
                        reply.success(JSONObject().put("width", Camera.WIDTH).put("height", Camera.HEIGHT).put("facing", facing).put("language", LANGUAGE))
                    } else {
                        reply.failure(failure, "cameraFailed", null)
                    }
                }
            }
        }

        context.register("stopCamera") { _, reply ->
            camera?.stop()
            camera = null
            reply.success(null)
        }

        context.register("takePhoto", { _, reply ->
            val current = camera ?: throw HaylenBridge.Failure("The camera is off. Call \"startCamera\" first.", "cameraOff", null)
            val photo = current.photo() ?: throw HaylenBridge.Failure("The camera has no frame yet.", "noFrame", null)
            reply.success(JSONObject().put("jpeg", photo).put("width", Camera.WIDTH).put("height", Camera.HEIGHT).put("language", LANGUAGE))
        }, HaylenBridge.Threading.BACKGROUND)
    }

    private fun registerMicrophone() {
        context.register("startMicrophone") { _, reply ->
            if (!context.application().packageManager.hasSystemFeature(PackageManager.FEATURE_MICROPHONE)) {
                throw HaylenBridge.Failure("This device has no microphone.", "unsupported", null)
            }
            withPermission(Manifest.permission.RECORD_AUDIO, reply) {
                microphone?.stop()
                microphone = null
                val record = Microphone.open() ?: throw HaylenBridge.Failure("The microphone of this device does not record.", "unsupported", null)
                val stream = context.openAudioStream("microphone", Microphone.SAMPLE_RATE, 1, HaylenAudioStream.Format.FLOAT32, Microphone.SAMPLE_RATE)
                microphone = Microphone(record, stream) { level -> context.emit("microphoneLevel", JSONObject().put("level", level).put("language", LANGUAGE)) }
                reply.success(JSONObject().put("sampleRate", Microphone.SAMPLE_RATE).put("channels", 1).put("language", LANGUAGE))
            }
        }

        context.register("stopMicrophone") { _, reply ->
            microphone?.stop()
            microphone = null
            reply.success(null)
        }
    }

    // One fix of the location, from the GPS where it is on and from the network otherwise.
    private fun registerLocation() {
        context.register("location") { _, reply ->
            withPermission(Manifest.permission.ACCESS_FINE_LOCATION, reply) { locate(reply) }
        }

        context.register("showMap") { params, reply ->
            val json = params as JSONObject
            val anchor = when (json.getString("anchor")) {
                "top" -> HaylenPlacement.Anchor.TOP
                "bottom" -> HaylenPlacement.Anchor.BOTTOM
                else -> throw HaylenBridge.Failure("The anchor of the map is \"top\" or \"bottom\", not \"${json.getString("anchor")}\".", "invalidAnchor", null)
            }
            val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            val placement = HaylenPlacement(anchor)
            placement.widthDp = 360
            placement.heightDp = 220
            placement.marginDp = 16f
            val latitude = json.getDouble("latitude")
            val longitude = json.getDouble("longitude")
            map?.remove()
            map = context.overlay().add(NativeDemoMap(activity, latitude, longitude), placement)
            reply.success(JSONObject().put("anchor", json.getString("anchor")).put("latitude", latitude).put("longitude", longitude).put("drawnWith", "an Android Canvas").put("language", LANGUAGE))
        }

        context.register("removeMap") { _, reply ->
            map?.remove()
            map = null
            reply.success(null)
        }
    }

    @SuppressLint("MissingPermission")
    private fun locate(reply: HaylenBridge.Reply) {
        val locations = context.application().getSystemService(LocationManager::class.java)
        val provider = listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER).firstOrNull { locations?.isProviderEnabled(it) == true }
        if (locations == null || provider == null) {
            reply.failure("This device has no location turned on.", "unsupported", null)
            return
        }
        val cancel = CancellationSignal()
        reply.onCancel { cancel.cancel() }
        LocationManagerCompat.getCurrentLocation(locations, provider, cancel, ContextCompat.getMainExecutor(context.application())) { location: Location? ->
            if (location == null) {
                reply.failure("The location is unknown.", "locationUnknown", null)
            } else {
                reply.success(JSONObject().put("latitude", location.latitude).put("longitude", location.longitude).put("accuracy", location.accuracy.toDouble()).put("language", LANGUAGE))
            }
        }
    }

    // The share sheet is an activity of the system, which tells the app nothing about what the person picked.
    private fun registerSharing() {
        context.register("share") { params, reply ->
            val json = params as JSONObject
            val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            val text = json.getString("text") + (json.optString("url").takeIf { it.isNotEmpty() }?.let { " $it" } ?: "")
            val send = Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text)
            activity.startActivity(Intent.createChooser(send, "Share"))
            reply.success(JSONObject().put("shared", JSONObject.NULL).put("language", LANGUAGE))
        }

        context.register("openApp") { params, reply ->
            val kind = (params as JSONObject).getString("kind")
            val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            val intent = when (kind) {
                "settings" -> Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", activity.packageName, null))
                "maps" -> Intent(Intent.ACTION_VIEW, Uri.parse("geo:-22.9519,-43.2105?q=-22.9519,-43.2105"))
                "mail" -> Intent(Intent.ACTION_SENDTO, Uri.parse("mailto:demo@example.com?subject=Native%20Demo"))
                else -> throw HaylenBridge.Failure("The app to open is \"settings\", \"maps\" or \"mail\", not \"$kind\".", "invalidApp", null)
            }
            val opened = try {
                activity.startActivity(intent)
                true
            } catch (missing: ActivityNotFoundException) {
                false
            }
            reply.success(JSONObject().put("opened", opened).put("language", LANGUAGE))
        }
    }

    // Runs `granted` once the app holds the permission, asking the person when it does not yet, and fails the call when the manifest lacks the permission, the person refuses it or `granted` throws.
    private fun withPermission(permission: String, reply: HaylenBridge.Reply, granted: () -> Unit) {
        context.requirements().require(HaylenRequirements.Requirement.permission(permission))
        if (ContextCompat.checkSelfPermission(context.application(), permission) == PackageManager.PERMISSION_GRANTED) {
            granted()
            return
        }
        ask(permission) { allowed ->
            try {
                if (allowed) granted() else reply.failure("The person has not allowed \"$permission\".", "permissionDenied", null)
            } catch (error: Exception) {
                reply.failure(error)
            }
        }
    }

    // The camera through Camera2, whose YUV frames of 640 by 480 pixels arrive on a thread of their own, which turns them into RGBA pixels, pushes them into the stream and keeps the newest one for a photo. It calls `started` once, with nothing once the capture runs or with the reason it does not.
    @SuppressLint("MissingPermission")
    private class Camera(context: Context, facing: String, private val stream: HaylenVideoStream, private val started: (String?) -> Unit) {
        private val thread = HandlerThread("native-demo-camera").apply { start() }
        private val handler = Handler(thread.looper)
        private val reader = ImageReader.newInstance(WIDTH, HEIGHT, ImageFormat.YUV_420_888, 2)
        private val pixels = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 4).order(ByteOrder.nativeOrder())
        private val newest = ByteArray(WIDTH * HEIGHT * 4)
        private var hasFrame = false
        private var device: CameraDevice? = null
        private val opened = System.nanoTime()

        init {
            val cameras = context.getSystemService(CameraManager::class.java)
            val wanted = if (facing == "front") CameraCharacteristics.LENS_FACING_FRONT else CameraCharacteristics.LENS_FACING_BACK
            val id = cameras.cameraIdList.firstOrNull { cameras.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_FACING) == wanted } ?: cameras.cameraIdList.first()
            reader.setOnImageAvailableListener({ source -> source.acquireLatestImage()?.use { convert(it) } }, handler)
            cameras.openCamera(id, object : CameraDevice.StateCallback() {
                override fun onOpened(camera: CameraDevice) {
                    device = camera
                    val configured = object : CameraCaptureSession.StateCallback() {
                        override fun onConfigured(session: CameraCaptureSession) {
                            val request = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW).apply { addTarget(reader.surface) }
                            session.setRepeatingRequest(request.build(), null, handler)
                            started(null)
                        }

                        override fun onConfigureFailed(session: CameraCaptureSession) {
                            started("The camera refused the capture session.")
                        }
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        camera.createCaptureSession(SessionConfiguration(SessionConfiguration.SESSION_REGULAR, listOf(OutputConfiguration(reader.surface)), { handler.post(it) }, configured))
                    } else {
                        @Suppress("DEPRECATION")
                        camera.createCaptureSession(listOf(reader.surface), configured, handler)
                    }
                }

                override fun onDisconnected(camera: CameraDevice) {
                    camera.close()
                }

                override fun onError(camera: CameraDevice, error: Int) {
                    camera.close()
                    started("The camera failed with the error $error.")
                }
            }, handler)
        }

        fun stop() {
            handler.post {
                device?.close()
                reader.close()
                thread.quitSafely()
            }
        }

        // Encodes the newest frame as a JPEG file.
        fun photo(): ByteArray? {
            val copy = synchronized(newest) { if (hasFrame) newest.copyOf() else null } ?: return null
            val bitmap = Bitmap.createBitmap(WIDTH, HEIGHT, Bitmap.Config.ARGB_8888)
            bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(copy))
            val jpeg = ByteArrayOutputStream()
            bitmap.compress(Bitmap.CompressFormat.JPEG, 80, jpeg)
            bitmap.recycle()
            return jpeg.toByteArray()
        }

        // Turns the planes of a YUV frame into RGBA pixels with the coefficients of BT.601.
        private fun convert(image: android.media.Image) {
            val (luma, blue, red) = image.planes
            val lumaBytes = luma.buffer
            val blueBytes = blue.buffer
            val redBytes = red.buffer
            pixels.clear()
            for (y in 0 until HEIGHT) {
                for (x in 0 until WIDTH) {
                    val brightness = (lumaBytes.get(y * luma.rowStride + x * luma.pixelStride).toInt() and 0xFF) - 16
                    val chroma = (y / 2) * blue.rowStride + (x / 2) * blue.pixelStride
                    val u = (blueBytes.get(chroma).toInt() and 0xFF) - 128
                    val v = (redBytes.get((y / 2) * red.rowStride + (x / 2) * red.pixelStride).toInt() and 0xFF) - 128
                    val scaled = 1.164f * brightness
                    pixels.put(clamp(scaled + 1.596f * v)).put(clamp(scaled - 0.392f * u - 0.813f * v)).put(clamp(scaled + 2.017f * u)).put(255.toByte())
                }
            }
            pixels.flip()
            synchronized(newest) {
                pixels.get(newest)
                hasFrame = true
            }
            pixels.rewind()
            stream.push(pixels, WIDTH, HEIGHT, WIDTH * 4, (System.nanoTime() - opened) / 1e9)
        }

        private fun clamp(value: Float): Byte = value.toInt().coerceIn(0, 255).toByte()

        companion object {
            const val WIDTH = 640
            const val HEIGHT = 480
        }
    }

    // The microphone through `AudioRecord`, whose mono floats a thread of its own reads in blocks of a hundredth of a second, pushes into the stream and measures about ten times per second.
    private class Microphone(private val record: AudioRecord, private val stream: HaylenAudioStream, private val level: (Double) -> Unit) {
        @Volatile
        private var running = true

        private val thread = Thread({
            val block = FloatArray(SAMPLE_RATE / 100)
            var squares = 0.0
            var measured = 0
            record.startRecording()
            while (running) {
                val frames = record.read(block, 0, block.size, AudioRecord.READ_BLOCKING)
                if (frames <= 0) {
                    continue
                }
                stream.push(block, frames)
                for (index in 0 until frames) {
                    squares += block[index] * block[index]
                }
                measured += frames
                if (measured >= SAMPLE_RATE / 10) {
                    level(sqrt(squares / measured))
                    squares = 0.0
                    measured = 0
                }
            }
            record.stop()
            record.release()
        }, "native-demo-microphone").apply { start() }

        fun stop() {
            running = false
        }

        companion object {
            const val SAMPLE_RATE = 44100

            // Returns a recorder of the microphone, or nothing when the device does not record, as an emulator without audio input.
            @SuppressLint("MissingPermission")
            fun open(): AudioRecord? {
                val size = maxOf(AudioRecord.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT), SAMPLE_RATE / 5 * 4)
                val record = AudioRecord(MediaRecorder.AudioSource.MIC, SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT, size)
                if (record.state == AudioRecord.STATE_INITIALIZED) {
                    return record
                }
                record.release()
                return null
            }
        }
    }

    private companion object {
        const val LANGUAGE = "Kotlin"
    }
}
