package com.haylen.plugins.{{PACKAGE}}

import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.text.TextUtils
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import dev.haylen.HaylenBridge
import dev.haylen.HaylenOverlay
import dev.haylen.HaylenPlacement
import dev.haylen.HaylenPluginContext
import org.json.JSONObject

// The banner of the {{TITLE}} plugin on Android: a native bar with a text and a button, which the overlay of the plugin places over the app at its top or bottom edge, and whose button sends the event `bannerTapped`. Touches outside the bar reach the app.
class {{NAME}}Banner(private val context: HaylenPluginContext) {
    private var panel: HaylenOverlay.Panel? = null
    private var taps = 0

    fun register() {
        context.register("showBanner") { params, reply ->
            val json = params as JSONObject
            val anchor = when (json.getString("anchor")) {
                "top" -> HaylenPlacement.Anchor.TOP
                "bottom" -> HaylenPlacement.Anchor.BOTTOM
                else -> throw HaylenBridge.Failure("The anchor of the banner is \"top\" or \"bottom\", not \"${json.getString("anchor")}\".", "invalidAnchor", null)
            }
            val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            val placement = HaylenPlacement(anchor)
            placement.reserve = json.getBoolean("reserve")
            placement.widthDp = 360
            placement.heightDp = 56
            val density = activity.resources.displayMetrics.density
            val bar = LinearLayout(activity)
            bar.orientation = LinearLayout.HORIZONTAL
            bar.gravity = Gravity.CENTER_VERTICAL
            bar.setPadding((16 * density).toInt(), 0, (8 * density).toInt(), 0)
            bar.background = GradientDrawable().apply {
                setColor(Color.rgb(88, 86, 214))
                cornerRadius = 10 * density
            }
            val label = TextView(activity)
            label.text = json.getString("text")
            label.setTextColor(Color.WHITE)
            label.textSize = 16f
            label.maxLines = 1
            label.ellipsize = TextUtils.TruncateAt.END
            bar.addView(label, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            val button = Button(activity)
            button.text = "Tap"
            button.setOnClickListener {
                taps += 1
                context.emit("bannerTapped", JSONObject().put("count", taps))
            }
            bar.addView(button, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
            panel?.remove()
            panel = context.overlay().add(bar, placement)
            reply.success(null)
        }

        context.register("hideBanner") { _, reply ->
            panel?.remove()
            panel = null
            reply.success(null)
        }
    }

    // The activity takes the views of the overlay with it.
    fun activityDestroyed() {
        panel = null
    }
}
