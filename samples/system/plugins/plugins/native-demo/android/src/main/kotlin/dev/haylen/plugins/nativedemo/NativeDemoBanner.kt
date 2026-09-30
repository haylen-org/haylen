package dev.haylen.plugins.nativedemo

import android.app.Activity
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.text.TextUtils
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import dev.haylen.HaylenOverlay
import dev.haylen.HaylenPlacement

// The native banner of the demo: a colored bar with the greeting and a Tap button, which the overlay of the plugin places over the app. Touches outside the bar reach the app.
class NativeDemoBanner(activity: Activity, text: String, color: Int, overlay: HaylenOverlay, placement: HaylenPlacement, tapped: () -> Unit) {
    private val panel: HaylenOverlay.Panel

    var isVisible = true
        set(value) {
            field = value
            panel.setVisible(value)
        }

    init {
        val density = activity.resources.displayMetrics.density
        val bar = LinearLayout(activity)
        bar.orientation = LinearLayout.HORIZONTAL
        bar.gravity = Gravity.CENTER_VERTICAL
        bar.setPadding((16 * density).toInt(), 0, (8 * density).toInt(), 0)
        bar.background = GradientDrawable().apply {
            setColor(color)
            cornerRadius = 10 * density
        }
        val label = TextView(activity)
        label.text = text
        label.setTextColor(Color.WHITE)
        label.textSize = 16f
        label.maxLines = 1
        label.ellipsize = TextUtils.TruncateAt.END
        bar.addView(label, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        val button = Button(activity)
        button.text = "Tap"
        button.setOnClickListener { tapped() }
        bar.addView(button, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        panel = overlay.add(bar, placement)
    }

    fun place(placement: HaylenPlacement) {
        panel.update(placement)
    }

    fun remove() {
        panel.remove()
    }

    companion object {
        const val WIDTH_DP = 360
        const val HEIGHT_DP = 56
    }
}
