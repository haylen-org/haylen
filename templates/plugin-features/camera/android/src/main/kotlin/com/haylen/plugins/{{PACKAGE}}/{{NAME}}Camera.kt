package com.haylen.plugins.{{PACKAGE}}

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.ImageFormat
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.params.OutputConfiguration
import android.hardware.camera2.params.SessionConfiguration
import android.media.Image
import android.media.ImageReader
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenVideoStream
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.json.JSONObject

// The camera of the {{TITLE}} plugin on Android: Camera2 into the video stream `camera`, and photos as JPEG bytes, after the person allowed the camera.
class {{NAME}}Camera(private val context: HaylenPluginContext, private val permissions: {{NAME}}Permissions) {
    private var capture: Capture? = null

    fun register() {
        context.register("startCamera") { params, reply ->
            val facing = (params as? JSONObject)?.optString("facing", "back") ?: "back"
            if (!context.application().packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) {
                throw HaylenBridge.Failure("This device has no camera.", "unsupported", null)
            }
            permissions.withPermission(context, Manifest.permission.CAMERA, reply) {
                val stream = context.openVideoStream("camera", Capture.WIDTH, Capture.HEIGHT, HaylenVideoStream.Format.RGBA8)
                capture?.stop()
                capture = Capture(context.application(), facing, stream) { failure ->
                    if (failure == null) {
                        reply.success(JSONObject().put("width", Capture.WIDTH).put("height", Capture.HEIGHT).put("facing", facing))
                    } else {
                        reply.failure(failure, "cameraFailed", null)
                    }
                }
            }
        }

        context.register("stopCamera") { _, reply ->
            capture?.stop()
            capture = null
            reply.success(null)
        }

        context.register("takePhoto", { _, reply ->
            val current = capture ?: throw HaylenBridge.Failure("The camera is off. Call \"startCamera\" first.", "cameraOff", null)
            val photo = current.photo() ?: throw HaylenBridge.Failure("The camera has no frame yet.", "noFrame", null)
            reply.success(JSONObject().put("jpeg", photo).put("width", Capture.WIDTH).put("height", Capture.HEIGHT))
        }, HaylenBridge.Threading.BACKGROUND)
    }

    // The capture of Camera2, whose YUV frames arrive on a thread of their own, which turns them into RGBA pixels, pushes them into the stream and keeps the newest one for a photo. It calls `started` once, with nothing once the capture runs or with the reason it does not.
    @SuppressLint("MissingPermission")
    private class Capture(context: Context, facing: String, private val stream: HaylenVideoStream, private val started: (String?) -> Unit) {
        private val thread = HandlerThread("{{ID}}-camera").apply { start() }
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
        private fun convert(image: Image) {
            val (luma, blue, red) = image.planes
            val lumaBytes = luma.buffer
            val blueBytes = blue.buffer
            val redBytes = red.buffer
            pixels.clear()
            for (y in 0 until HEIGHT) {
                for (x in 0 until WIDTH) {
                    val brightness = (lumaBytes.get(y * luma.rowStride + x * luma.pixelStride).toInt() and 0xFF) - 16
                    val u = (blueBytes.get((y / 2) * blue.rowStride + (x / 2) * blue.pixelStride).toInt() and 0xFF) - 128
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
}
