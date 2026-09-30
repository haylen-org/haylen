package dev.haylen.plugins.nativedemo

import android.app.Activity
import android.app.Dialog
import android.graphics.Color
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

// A native screen over the whole app, with a title, a line of text and a Close button, shown as a full screen dialog. The Back button closes it too, and the directional pad of gamepads and TV remotes starts on Close.
class NativeDemoScreen(activity: Activity, title: String, color: Int, closed: () -> Unit) {
    private val dialog = Dialog(activity, android.R.style.Theme_DeviceDefault_NoActionBar_Fullscreen)

    init {
        val density = activity.resources.displayMetrics.density
        val column = LinearLayout(activity)
        column.orientation = LinearLayout.VERTICAL
        column.gravity = Gravity.CENTER
        column.setBackgroundColor(color)
        val padding = (32 * density).toInt()
        column.setPadding(padding, padding, padding, padding)
        val heading = TextView(activity)
        heading.text = title
        heading.textSize = 32f
        heading.setTextColor(Color.WHITE)
        heading.gravity = Gravity.CENTER
        column.addView(heading)
        val detail = TextView(activity)
        detail.text = "This Android dialog covers the app, which stands still and stays silent until Close ends the cover."
        detail.textSize = 18f
        detail.setTextColor(Color.WHITE)
        detail.gravity = Gravity.CENTER
        detail.setPadding(0, padding / 2, 0, padding / 2)
        column.addView(detail)
        val close = Button(activity)
        close.text = "Close"
        close.setOnClickListener { dialog.dismiss() }
        column.addView(close, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        dialog.setContentView(column)
        dialog.setOnDismissListener { closed() }
        dialog.setOnShowListener { close.requestFocus() }
    }

    fun show() {
        dialog.show()
    }
}
