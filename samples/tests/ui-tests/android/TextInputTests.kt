package com.haylen.uitests

import android.content.Intent
import android.text.InputType
import androidx.test.uiautomator.By
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.util.regex.Pattern

// The address field of the test "Text fields" types through the keys of the keyboard of the system: each key types once, Backspace deletes, the keyboard stays open when the field is tapped again, comes back after it hid, and the field keeps its text and its focus when the device turns. The keyboard covers the status line of the test on a phone in landscape, so the tests read the text of the field itself. A field without autocorrection asks the keyboard for no suggestions, and the text area, which takes autocorrection, asks for corrections.
class TextInputTests : HaylenUiTestCase() {
    companion object {
        const val FIELD = "https://haylen.dev"
        // The time during which the tests watch that the keyboard stays, after a tap or a turn that could close it.
        const val WATCH = 1_500L
        const val DIARY = "Dear diary,"
    }

    override val launchArguments = listOf("--test", "GUI-005")

    // The test focuses its first field, which edits and opens the keyboard over the address field on a phone in landscape, so the back button hides the keyboard before the tests tap the address field, as a person does.
    @Before
    fun openTheAddressField() {
        waitForKeyboard()
        device.pressBack()
        assertTrue("The keyboard stays open after the back button.", poll { !isKeyboardShown() })
        startTyping()
    }

    @Test
    fun eachKeyTypesOnceAndBackspaceDeletes() {
        type("oi")
        expectText("oi")
        pressKey("Delete")
        expectText("o")
        type("la")
        expectText("ola")
        saveScreenshot("text-typed")
    }

    @Test
    fun theKeyboardStaysOpenWhenTheFieldIsTappedAgain() {
        type("mar")
        expectText("mar")
        waitFor(FIELD).click()
        watchTheKeyboard("The keyboard closes when the field that edits is tapped again.")
        type("e")
        expectText("mare")
        saveScreenshot("keyboard-stays")
    }

    @Test
    fun theKeyboardComesBackAfterItHid() {
        type("ilha")
        expectText("ilha")

        // The back button hides the keyboard while the field goes on editing, and a tap on the field shows it again.
        device.pressBack()
        assertTrue("The keyboard stays open after the back button.", poll { !isKeyboardShown() })
        startTyping()
        type("s")
        expectText("ilhas")

        // The keyboard comes back with the app.
        goHome()
        returnToApp()
        waitForKeyboard()
        type("e")
        expectText("ilhase")
        saveScreenshot("keyboard-back")
    }

    // The test project shows only landscape, so the device turns from one side of landscape to the other, which the app follows by the sensor.
    @Test
    fun theFieldKeepsItsTextAndItsFocusWhenTheDeviceTurns() {
        type("mar")
        expectText("mar")
        turnDevice((device.displayRotation + 2) % 4)
        watchTheKeyboard("The keyboard closes when the device turns.")
        expectText("mar")
        type("e")
        expectText("mare")
        saveScreenshot("text-turned")
    }

    @Test
    fun aFieldAsksForSuggestionsOnlyWithAutocorrection() {
        val field = editorInputType()
        assertEquals("The address field asks the keyboard for suggestions.", InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS, field and InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS)
        assertEquals("The address field asks the keyboard for corrections.", 0, field and InputType.TYPE_TEXT_FLAG_AUTO_CORRECT)

        device.pressBack()
        assertTrue("The keyboard stays open after the back button.", poll { !isKeyboardShown() })
        waitFor(Pattern.compile(Pattern.quote(DIARY) + ".*", Pattern.DOTALL)).click()
        waitForKeyboard()
        var area = 0
        assertTrue("The text area never edits with the keyboard of text areas.", poll {
            area = editorInputType()
            area and InputType.TYPE_TEXT_FLAG_MULTI_LINE != 0
        })
        assertEquals("The text area, which takes autocorrection, asks the keyboard for no suggestions.", 0, area and InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS)
        assertEquals("The text area, which takes autocorrection, asks the keyboard for no corrections.", InputType.TYPE_TEXT_FLAG_AUTO_CORRECT, area and InputType.TYPE_TEXT_FLAG_AUTO_CORRECT)
    }

    // The input type of the field that the keyboard edits, as the input method service of the system reports it.
    private fun editorInputType(): Int {
        val report = device.executeShellCommand("dumpsys input_method")
        val editor = Regex("curEditorInfo:\\s+inputType=0x([0-9a-fA-F]+)").find(report) ?: error("The input method service reports no field that the keyboard edits.")
        return editor.groupValues[1].toLong(16).toInt()
    }

    private fun startTyping() {
        waitFor(FIELD).click()
        waitForKeyboard()
    }

    private fun keyboard() = By.pkg(device.executeShellCommand("settings get secure default_input_method").trim().substringBefore("/"))

    // Taps the key of the keyboard of the system whose content description is the name, such as "o" or "Delete".
    private fun pressKey(name: String) {
        waitFor(keyboard().desc(name), "the key \"$name\" of the keyboard").click()
    }

    // Types the letters with the keys of the keyboard of the system, one tap each.
    private fun type(text: String) {
        for (letter in text) {
            pressKey(letter.toString())
        }
    }

    // Fails when the keyboard closes while the test watches it.
    private fun watchTheKeyboard(message: String) {
        val deadline = System.currentTimeMillis() + WATCH
        while (System.currentTimeMillis() < deadline) {
            assertTrue(message, isKeyboardShown())
            Thread.sleep(POLL)
        }
    }

    // Brings the app back to the front the way its icon on the launcher does, which resumes its task without a new start.
    private fun returnToApp() {
        val context = instrumentation.context
        val intent = context.packageManager.getLaunchIntentForPackage(identifier) ?: error("The app \"$identifier\" is not installed.")
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        context.startActivity(intent)
        waitForApp()
    }

    private fun expectText(text: String) {
        var shown: String? = null
        val settled = poll {
            shown = labelOf(element(FIELD))
            shown == text
        }
        assertTrue("The address field shows \"$shown\" instead of \"$text\". ${describeScreen()}", settled)
    }
}
