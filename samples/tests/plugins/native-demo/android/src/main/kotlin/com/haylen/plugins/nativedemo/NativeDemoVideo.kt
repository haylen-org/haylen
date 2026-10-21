package com.haylen.plugins.nativedemo

import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import dev.haylen.HaylenVideoStream

// The picture of the demo in the video stream, with its waves moving and its sun floating, which a `Canvas` draws 30 times per second into a `Bitmap` on a thread of its own and pushes into the stream, the way a camera or a video decoder delivers its frames.
class NativeDemoVideo(private val stream: HaylenVideoStream) {
    private val thread = HandlerThread("native-demo-video").apply { start() }
    private val handler = Handler(thread.looper)
    private val bitmap = Bitmap.createBitmap(WIDTH, HEIGHT, Bitmap.Config.ARGB_8888)
    private val canvas = Canvas(bitmap)
    private val scene = NativeDemoScene()
    private val started = SystemClock.uptimeMillis()
    private var frame = 0

    // Each frame is due at its time since the start, so the video keeps 30 frames per second however long drawing takes.
    private val draw = object : Runnable {
        override fun run() {
            scene.draw(canvas, WIDTH.toFloat(), HEIGHT.toFloat(), frame.toFloat() / FPS)
            stream.push(bitmap, frame.toDouble() / FPS)
            frame += 1
            handler.postAtTime(this, started + frame * 1000L / FPS)
        }
    }

    init {
        handler.post(draw)
    }

    fun stop() {
        handler.removeCallbacks(draw)
        thread.quitSafely()
    }

    companion object {
        const val WIDTH = 320
        const val HEIGHT = 180
        const val FPS = 30
    }
}
