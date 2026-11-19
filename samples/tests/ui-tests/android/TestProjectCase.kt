package com.haylen.uitests

import androidx.test.uiautomator.UiObject2
import java.util.regex.Pattern
import org.junit.Assert.assertTrue
import org.junit.Before

// The base of the UI tests that open one test of the test project, which the launch arguments "--test <code>" name, and read its checks. Each check is a node whose content description is the line of its row, such as "Pass: The camera starts. The camera started at 640 by 480 pixels.".
abstract class TestProjectCase : HaylenUiTestCase() {
    companion object {
        // The resource ids of the buttons of the permission dialog that allow what the plugin asks for.
        val ALLOW = arrayOf("permission_allow_button")
        val ALLOW_WHILE_USING = arrayOf("permission_allow_foreground_only_button")
    }

    // The code of the test that the app opens, such as "PLG-016".
    abstract val code: String

    override val launchArguments: List<String>
        get() = listOf("--test", code)

    @Before
    fun waitForTest() {
        waitFor(code)
    }

    // The node of the check with the name, in whatever state it is.
    fun check(name: String): UiObject2? = element(Pattern.compile("(Waiting|Pass|Fail|Info|N/A): ${Pattern.quote(name)}\\..*", Pattern.DOTALL))

    // Waits until the check with the name reaches the state with a line that holds the text, and fails at once when the check fails instead. It returns the line of the check.
    fun expectCheck(name: String, state: String = "Pass", containing: String = "", timeout: Long = TIMEOUT): String {
        val expected = "$state: $name."
        var line = ""
        val settled = poll(timeout) {
            line = labelOf(check(name)) ?: ""
            line.startsWith("Fail: ") || (line.startsWith(expected) && line.contains(containing))
        }
        assertTrue("The check \"$name\" does not reach \"$state\" with \"$containing\". It reads \"$line\".", settled && line.startsWith(expected))
        return line
    }
}
