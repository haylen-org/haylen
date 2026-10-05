package dev.haylen.plugins.nativedemo

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.view.View
import java.util.Locale
import kotlin.math.floor

// The map view of the demo, which draws what a view of a map SDK shows without the SDK or the network: the sea, a coast, a grid of the degrees around the place, roads and a pin on the place with its coordinates.
class NativeDemoMap(context: Context, private val latitude: Double, private val longitude: Double) : View(context) {
    private val density = context.resources.displayMetrics.density
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { textSize = 12 * density }
    private val shape = Path()
    private val bounds = RectF()

    override fun onDraw(canvas: Canvas) {
        val width = width.toFloat()
        val height = height.toFloat()
        val radius = 12 * density
        bounds.set(0f, 0f, width, height)
        fill.color = Color.rgb(170, 211, 223)
        canvas.drawRoundRect(bounds, radius, radius, fill)

        // The land of a coast that the place sits on, shaped by its coordinates so every place looks a little different.
        val wave = ((latitude - floor(latitude)) * height / 4).toFloat()
        shape.reset()
        shape.moveTo(0f, height * 0.35f + wave)
        shape.cubicTo(width * 0.3f, height * 0.15f, width * 0.6f, height * 0.6f - wave, width, height * 0.3f)
        shape.lineTo(width, height)
        shape.lineTo(0f, height)
        shape.close()
        fill.color = Color.rgb(232, 225, 202)
        canvas.drawPath(shape, fill)

        line.color = Color.argb(60, 0, 0, 0)
        line.strokeWidth = density
        for (step in 1 until 6) {
            canvas.drawLine(width * step / 6, 0f, width * step / 6, height, line)
            canvas.drawLine(0f, height * step / 6, width, height * step / 6, line)
        }
        line.color = Color.WHITE
        line.strokeWidth = 4 * density
        canvas.drawLine(0f, height * 0.75f, width, height * 0.55f, line)
        canvas.drawLine(width * 0.6f, height, width * 0.45f, height * 0.4f, line)

        fill.color = Color.rgb(219, 68, 55)
        canvas.drawCircle(width / 2, height / 2 - 10 * density, 9 * density, fill)
        shape.reset()
        shape.moveTo(width / 2 - 8 * density, height / 2 - 6 * density)
        shape.lineTo(width / 2 + 8 * density, height / 2 - 6 * density)
        shape.lineTo(width / 2, height / 2 + 8 * density)
        shape.close()
        canvas.drawPath(shape, fill)
        fill.color = Color.WHITE
        canvas.drawCircle(width / 2, height / 2 - 10 * density, 3.5f * density, fill)

        text.color = Color.rgb(32, 33, 36)
        canvas.drawText(String.format(Locale.ROOT, "%.4f, %.4f", latitude, longitude), 10 * density, height - 10 * density, text)
    }
}
