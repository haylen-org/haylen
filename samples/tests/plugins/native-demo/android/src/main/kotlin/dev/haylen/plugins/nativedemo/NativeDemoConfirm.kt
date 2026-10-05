package dev.haylen.plugins.nativedemo

import android.app.Activity
import android.content.Context
import android.content.Intent
import androidx.activity.result.contract.ActivityResultContract
import org.json.JSONObject

// The Activity Result contract of the confirm screen, which starts `NativeDemoConfirmActivity` with the title, the question and the color of the screen, and gives `true` when the person confirms, `false` when the person declines and `null` when the person goes back.
class NativeDemoConfirm : ActivityResultContract<JSONObject, Boolean?>() {
    override fun createIntent(context: Context, input: JSONObject): Intent =
        Intent(context, NativeDemoConfirmActivity::class.java).putExtra(TITLE, input.optString("title")).putExtra(QUESTION, input.optString("question")).putExtra(COLOR, input.optInt("color"))

    override fun parseResult(resultCode: Int, intent: Intent?): Boolean? = if (resultCode == Activity.RESULT_OK && intent != null) intent.getBooleanExtra(CONFIRMED, false) else null

    companion object {
        const val TITLE = "dev.haylen.plugins.nativedemo.TITLE"
        const val QUESTION = "dev.haylen.plugins.nativedemo.QUESTION"
        const val COLOR = "dev.haylen.plugins.nativedemo.COLOR"
        const val CONFIRMED = "dev.haylen.plugins.nativedemo.CONFIRMED"
    }
}
