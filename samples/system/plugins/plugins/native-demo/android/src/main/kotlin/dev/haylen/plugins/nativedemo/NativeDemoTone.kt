package dev.haylen.plugins.nativedemo

import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import dev.haylen.HaylenAudioStream
import kotlin.math.PI
import kotlin.math.sin

// A sine wave that a thread of its own synthesizes a tenth of a second ahead of the clock into the audio stream, the way a synthesized voice or decoded network audio arrives, as mono 32-bit floats at 44100 Hz.
class NativeDemoTone(frequency: Double, private val stream: HaylenAudioStream) {
    private val thread = HandlerThread("native-demo-tone").apply { start() }
    private val handler = Handler(thread.looper)
    private val step = 2 * PI * frequency / SAMPLE_RATE
    private val started = SystemClock.uptimeMillis()
    private val block = FloatArray(SAMPLE_RATE / 100)
    private var written = 0L
    private var phase = 0.0

    private val fill = object : Runnable {
        override fun run() {
            val target = ((SystemClock.uptimeMillis() - started) / 1000.0 + 0.1) * SAMPLE_RATE
            while (written < target) {
                for (index in block.indices) {
                    block[index] = (sin(phase) * 0.3).toFloat()
                    phase = (phase + step) % (2 * PI)
                }
                stream.push(block, block.size)
                written += block.size
            }
            handler.postDelayed(this, 10)
        }
    }

    init {
        handler.post(fill)
    }

    fun stop() {
        handler.removeCallbacks(fill)
        thread.quitSafely()
    }

    companion object {
        const val SAMPLE_RATE = 44100
    }
}
