package dev.haylen.plugins.nativedemo

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Shader
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import dev.haylen.HaylenVideoStream

// The animated pattern of the video stream: stripes that move over a gradient whose colors turn, which a `Canvas` draws 30 times per second into a `Bitmap` on a thread of its own and pushes into the stream, the way a camera or a video decoder delivers its frames.
class NativeDemoVideo(private val stream: HaylenVideoStream) {
    private val thread = HandlerThread("native-demo-video").apply { start() }
    private val handler = Handler(thread.looper)
    private val bitmap = Bitmap.createBitmap(WIDTH, HEIGHT, Bitmap.Config.ARGB_8888)
    private val canvas = Canvas(bitmap)
    private val gradient = Paint()
    private val stripe = Paint().apply { color = Color.argb(89, 255, 255, 255) }
    private val path = Path()
    private val started = SystemClock.uptimeMillis()
    private var frame = 0

    // Each frame is due at its time since the start, so the pattern keeps 30 frames per second however long drawing takes.
    private val draw = object : Runnable {
        override fun run() {
            paint()
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

    private fun paint() {
        val width = WIDTH.toFloat()
        val height = HEIGHT.toFloat()
        val turn = (frame % 180) / 180f
        gradient.shader = LinearGradient(0f, 0f, width, height, Color.rgb(turn, 0.3f, 1 - turn), Color.rgb(1 - turn, 0.8f, turn), Shader.TileMode.CLAMP)
        canvas.drawPaint(gradient)

        path.reset()
        var offset = -height + (frame % 32) * 2
        while (offset < width) {
            path.moveTo(offset, 0f)
            path.lineTo(offset + 16, 0f)
            path.lineTo(offset + 16 + height, height)
            path.lineTo(offset + height, height)
            path.close()
            offset += 64
        }
        canvas.drawPath(path, stripe)
    }

    companion object {
        const val WIDTH = 320
        const val HEIGHT = 180
        const val FPS = 30
    }
}
