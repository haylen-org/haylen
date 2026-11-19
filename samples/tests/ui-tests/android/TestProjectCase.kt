package com.haylen.uitests

import android.graphics.Rect
import androidx.test.uiautomator.By
import androidx.test.uiautomator.StaleObjectException
import androidx.test.uiautomator.UiObject2
import java.util.regex.Pattern
import org.junit.Assert.assertTrue
import org.junit.Before

// The base of the UI tests that open one test of the test project, which the launch arguments "--test <code>" name, and read its checks. Each check is a node whose content description is the line of its row, such as "Pass: The camera starts. The camera started at 640 by 480 pixels.".
abstract class TestProjectCase : HaylenUiTestCase() {
    companion object {
        // The resource ids of the buttons of the permission dialog that allow what the plugin asks for.
        val ALLOW = arrayOf("permission_allow_button")
        // The camera and the microphone, which Android 11 and later offer while the app is in use and Android 10 with a plain "Allow".
        val ALLOW_WHILE_USING = arrayOf("permission_allow_foreground_only_button", "permission_allow_button")
        // The location, which every version offers while the app is in use.
        val ALLOW_LOCATION_WHILE_USING = arrayOf("permission_allow_foreground_only_button")
        // Every check, whatever its name, by the state that starts its line.
        val ANY_CHECK: Pattern = Pattern.compile("(Waiting|Pass|Fail|Info|N/A): .*", Pattern.DOTALL)
        // The most drags that the search of a check makes toward each end of the list of checks, more than any test has rows.
        const val MAX_DRAGS = 12
        // A drag moves the list by this share of the height that its cards show, slowly, so the list follows the finger without a fling.
        const val DRAG_SHARE = 0.5
        const val DRAG_STEPS = 60
        // The longest that the list takes to come to rest after a drag.
        const val SETTLE_TIMEOUT = 5_000L
    }

    // The code of the test that the app opens, such as "PLG-016".
    abstract val code: String

    override val launchArguments: List<String>
        get() = listOf("--test", code)

    @Before
    fun waitForTest() {
        waitFor(code)
    }

    // The node of the check with the name, in whatever state it is. The accessibility tree holds only the cards that the list of checks shows, so a check that the list keeps out of view comes into view the way a person finds it, by dragging the list toward each end.
    fun check(name: String): UiObject2? {
        val pattern = Pattern.compile("(Waiting|Pass|Fail|Info|N/A): ${Pattern.quote(name)}\\..*", Pattern.DOTALL)
        return element(pattern) ?: revealCheck(pattern)
    }

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

    // Drags the list of checks toward its end and then toward its start until the check shows or the list stops moving, and returns its node, or null when the list holds no such check yet or shows no check at all, such as while a native screen covers the app.
    private fun revealCheck(pattern: Pattern): UiObject2? {
        for (towardEnd in listOf(true, false)) {
            var shown = visibleChecks()
            for (drag in 1..MAX_DRAGS) {
                val area = areaOf(shown) ?: return null
                dragChecks(area, towardEnd)
                val moved = settledChecks()
                val found = element(pattern)
                if (found != null) {
                    return found
                }

                if (moved == shown) {
                    break
                }

                shown = moved
            }
        }
        return null
    }

    // The cards of the checks that the screen shows, each as its place and the start of its line, which tells whether a drag moved the list.
    private fun visibleChecks(): List<Pair<Rect, String>> {
        val cards = mutableListOf<Pair<Rect, String>>()
        for (node in device.findObjects(By.desc(ANY_CHECK))) {
            try {
                cards.add(Pair(node.visibleBounds, node.contentDescription.substringBefore(". ")))
            } catch (stale: StaleObjectException) {
                continue
            }
        }
        return cards
    }

    // Reads the cards until two reads agree, once the list came to rest after a drag.
    private fun settledChecks(): List<Pair<Rect, String>> {
        var last = visibleChecks()
        poll(SETTLE_TIMEOUT) {
            Thread.sleep(POLL)
            val current = visibleChecks()
            val resting = current == last
            last = current
            resting
        }
        return last
    }

    // The part of the screen that the cards of the checks cover, which lies inside the list that scrolls them.
    private fun areaOf(cards: List<Pair<Rect, String>>): Rect? {
        if (cards.isEmpty()) {
            return null
        }

        val area = Rect(cards.first().first)
        for ((bounds, _) in cards) {
            area.union(bounds)
        }
        return area
    }

    // Drags a finger up the list to bring the checks after the ones that show into view, or down to bring the ones before them.
    private fun dragChecks(area: Rect, towardEnd: Boolean) {
        val distance = (area.height() * DRAG_SHARE).toInt()
        val top = area.centerY() - distance / 2
        val bottom = top + distance
        val from = if (towardEnd) bottom else top
        val to = if (towardEnd) top else bottom
        device.swipe(area.centerX(), from, area.centerX(), to, DRAG_STEPS)
    }
}
