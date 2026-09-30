package dev.haylen.plugins.nativedemo

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat

// The confirm screen of the demo on Android: an AndroidX activity of its own with the question of the app and the Confirm and Decline buttons, whose answer returns through the Activity Result API. The Back button ends it without an answer, and the directional pad of gamepads and TV remotes starts on Confirm.
class NativeDemoConfirmActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val density = resources.displayMetrics.density
        val padding = (32 * density).toInt()
        val column = LinearLayout(this)
        column.orientation = LinearLayout.VERTICAL
        column.gravity = Gravity.CENTER
        column.setBackgroundColor(intent.getIntExtra(NativeDemoConfirm.COLOR, Color.DKGRAY))
        ViewCompat.setOnApplyWindowInsetsListener(column) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            view.setPadding(padding + bars.left, padding + bars.top, padding + bars.right, padding + bars.bottom)
            insets
        }

        column.addView(text(intent.getStringExtra(NativeDemoConfirm.TITLE), 32f))
        val question = text(intent.getStringExtra(NativeDemoConfirm.QUESTION), 20f)
        question.setPadding(0, padding / 2, 0, padding / 2)
        column.addView(question)
        val buttons = LinearLayout(this)
        buttons.orientation = LinearLayout.HORIZONTAL
        buttons.gravity = Gravity.CENTER
        buttons.addView(button("Decline", false))
        val confirm = button("Confirm", true)
        buttons.addView(confirm)
        column.addView(buttons)
        setContentView(column)
        confirm.requestFocus()
    }

    private fun text(value: String?, size: Float): TextView {
        val view = TextView(this)
        view.text = value
        view.textSize = size
        view.setTextColor(Color.WHITE)
        view.gravity = Gravity.CENTER
        return view
    }

    private fun button(label: String, confirmed: Boolean): Button {
        val view = Button(this)
        view.text = label
        view.setOnClickListener {
            setResult(Activity.RESULT_OK, Intent().putExtra(NativeDemoConfirm.CONFIRMED, confirmed))
            finish()
        }
        return view
    }
}
