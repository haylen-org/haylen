package com.haylen.plugins.nativedemo

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Shader
import kotlin.math.cos
import kotlin.math.sin

// The picture of the demo, a small island with a palm and its coconuts under the sun and two clouds, in a sea with waves, in fractions of the width and the height of the canvas, the same on every platform. Every shape has the outline of the art, the sun a soft halo, the sand and the leaves a lighter highlight and the island a shadow on the water. The video moves the waves and floats the sun with the time in seconds. Each thread that draws keeps a scene of its own, since the scene reuses its paints.
class NativeDemoScene {
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
    }
    private val oval = RectF()

    fun draw(canvas: Canvas, width: Float, height: Float, seconds: Float) {
        val horizon = 0.6f * height
        val outline = 0.012f * height
        fill.shader = LinearGradient(0f, 0f, 0f, horizon, SKY_TOP, SKY_LOW, Shader.TileMode.CLAMP)
        canvas.drawRect(0f, 0f, width, horizon, fill)
        fill.shader = LinearGradient(0f, horizon, 0f, height, SEA_TOP, SEA_LOW, Shader.TileMode.CLAMP)
        canvas.drawRect(0f, horizon, width, height, fill)
        fill.shader = null

        val sunY = 0.26f * height + 0.02f * height * sin(seconds * 1.2f)
        fill.color = HALO
        canvas.drawCircle(0.8f * width, sunY, 0.17f * height, fill)
        outlinedEllipse(canvas, 0.8f * width, sunY, 0.12f * height, 0.12f * height, 0f, outline, SUN)
        fill.color = SUN_LIGHT
        canvas.drawCircle(0.78f * width, sunY - 0.02f * height, 0.07f * height, fill)
        for (circles in CLOUDS) {
            drawCloud(canvas, circles, width, height, outline)
        }

        val spacing = 0.2f * width
        stroke.strokeWidth = 0.014f * height
        stroke.color = Color.argb(153, 255, 255, 255)
        for (row in 0 until 3) {
            val waveY = horizon + (0.12f + 0.12f * row) * height
            var start = (seconds * 0.06f * width * (1 + 0.5f * row) + 0.07f * width * row) % spacing - spacing
            while (start < width) {
                canvas.drawLine(start + 0.01f * width, waveY, start + 0.1f * width, waveY, stroke)
                start += spacing
            }
        }

        filledEllipse(canvas, 0.36f * width, 0.69f * height, 0.24f * width, 0.05f * height, 0f, SHADOW)
        outlinedEllipse(canvas, 0.36f * width, 0.64f * height, 0.22f * width, 0.085f * height, 0f, outline, SAND)
        filledEllipse(canvas, 0.24f * width, 0.675f * height, 0.05f * width, 0.013f * height, 0f, SAND_LIGHT)
        outlinedEllipse(canvas, 0.36f * width, 0.6f * height, 0.15f * width, 0.05f * height, 0f, outline, GRASS)
        val crownX = 0.42f * width
        val crownY = 0.3f * height
        stroke.strokeWidth = 0.03f * height + outline * 2
        stroke.color = INK
        canvas.drawLine(0.38f * width, 0.6f * height, crownX, crownY, stroke)
        stroke.strokeWidth = 0.03f * height
        stroke.color = TRUNK
        canvas.drawLine(0.38f * width, 0.6f * height, crownX, crownY, stroke)
        for (angle in LEAF_ANGLES) {
            val leafX = crownX + cos(angle) * 0.085f * width
            val leafY = crownY + sin(angle) * 0.085f * width
            outlinedEllipse(canvas, leafX, leafY, 0.09f * width, 0.028f * height, angle, outline, LEAF)
            filledEllipse(canvas, leafX, leafY, 0.07f * width, 0.008f * height, angle, LEAF_LIGHT)
        }
        for (coconut in COCONUTS) {
            outlinedEllipse(canvas, coconut[0] * width, coconut[1] * height, 0.025f * height, 0.025f * height, 0f, outline, COCONUT)
        }
    }

    // A cloud strokes the outlines of all its circles before it fills them, so one outline goes around the whole cloud.
    private fun drawCloud(canvas: Canvas, circles: Array<FloatArray>, width: Float, height: Float, outline: Float) {
        stroke.strokeWidth = outline * 2
        stroke.color = INK
        for (circle in circles) {
            canvas.drawCircle(circle[0] * width, circle[1] * height, circle[2] * height, stroke)
        }
        fill.color = CLOUD
        for (circle in circles) {
            canvas.drawCircle(circle[0] * width, circle[1] * height, circle[2] * height, fill)
        }
    }

    private fun filledEllipse(canvas: Canvas, x: Float, y: Float, radiusX: Float, radiusY: Float, angle: Float, color: Int) {
        canvas.save()
        canvas.rotate(Math.toDegrees(angle.toDouble()).toFloat(), x, y)
        oval.set(x - radiusX, y - radiusY, x + radiusX, y + radiusY)
        fill.color = color
        canvas.drawOval(oval, fill)
        canvas.restore()
    }

    // Fills an ellipse over the outline of the art, a stroke twice as wide as the outline whose inner half the fill covers.
    private fun outlinedEllipse(canvas: Canvas, x: Float, y: Float, radiusX: Float, radiusY: Float, angle: Float, outline: Float, color: Int) {
        canvas.save()
        canvas.rotate(Math.toDegrees(angle.toDouble()).toFloat(), x, y)
        oval.set(x - radiusX, y - radiusY, x + radiusX, y + radiusY)
        stroke.strokeWidth = outline * 2
        stroke.color = INK
        canvas.drawOval(oval, stroke)
        fill.color = color
        canvas.drawOval(oval, fill)
        canvas.restore()
    }

    private companion object {
        val INK = Color.rgb(0x1E, 0x1B, 0x33)
        val SKY_TOP = Color.rgb(0x6C, 0xC6, 0xF5)
        val SKY_LOW = Color.rgb(0xCF, 0xF1, 0xFF)
        val SEA_TOP = Color.rgb(0x2F, 0xB3, 0xC4)
        val SEA_LOW = Color.rgb(0x1E, 0x8F, 0xA6)
        val SUN = Color.rgb(0xFF, 0xC9, 0x3C)
        val SUN_LIGHT = Color.rgb(0xFF, 0xE8, 0x9A)
        val SAND = Color.rgb(0xF3, 0xD5, 0x8D)
        val GRASS = Color.rgb(0x7B, 0xC7, 0x4D)
        val TRUNK = Color.rgb(0xA8, 0x6A, 0x3D)
        val LEAF = Color.rgb(0x5D, 0xB3, 0x3C)
        val LEAF_LIGHT = Color.rgb(0x8F, 0xD4, 0x5E)
        val SAND_LIGHT = Color.rgb(0xFB, 0xE8, 0xB6)
        val COCONUT = Color.rgb(0x6B, 0x44, 0x23)
        val CLOUD = Color.rgb(0xFF, 0xFF, 0xFF)
        val HALO = Color.argb(115, 0xFF, 0xFA, 0xE1)
        val SHADOW = Color.argb(46, 0x1E, 0x1B, 0x33)
        val LEAF_ANGLES = floatArrayOf(-0.45f, 0.4f, 2.75f, 3.55f)

        // Each cloud is three circles, with the x and y of their centers in fractions of the width and the height and their radius in fractions of the height.
        val CLOUDS = arrayOf(
            arrayOf(floatArrayOf(0.14f, 0.22f, 0.06f), floatArrayOf(0.19f, 0.17f, 0.08f), floatArrayOf(0.245f, 0.22f, 0.06f)),
            arrayOf(floatArrayOf(0.56f, 0.15f, 0.05f), floatArrayOf(0.6f, 0.12f, 0.065f), floatArrayOf(0.64f, 0.15f, 0.05f)),
        )
        val COCONUTS = arrayOf(floatArrayOf(0.41f, 0.33f), floatArrayOf(0.435f, 0.335f))
    }
}
