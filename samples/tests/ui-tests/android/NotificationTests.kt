package com.haylen.uitests

import android.Manifest.permission.POST_NOTIFICATIONS
import android.os.Build
import java.util.regex.Pattern
import org.junit.Test

// PLG-025: the screen of the plugin shows the permission dialog of notifications, and the tap on a notification reaches the app while it shows, from the home screen and after the app left the recent apps. The test takes the permission back before it starts, so the dialog shows from Android 13 on.
class NotificationTests : TestProjectCase() {
    companion object {
        const val NOTIFICATION = "Tap to open Haylen Tests."
        const val SCHEDULED_NAME = "A local notification is scheduled"
        const val TAP_NAME = "The tap on the notification reaches the app"
    }

    override val code = "PLG-025"
    override val revokedPermissions = listOf(POST_NOTIFICATIONS)

    @Test
    fun theNotificationTapsReachTheApp() {
        scheduleNotification(asks = Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
        tapNotification(NOTIFICATION)
        expectCheck(TAP_NAME, containing = "received 1 taps")
        saveScreenshot("notification")

        scheduleNotification(asks = false)
        goHome()
        tapNotification(NOTIFICATION)
        expectCheck(TAP_NAME, containing = "received 2 taps")

        // The system starts the app without launch arguments, so the test opens again through the search of the menu. The alarm of the notification wakes the process of the app, so the app ends only once the notification is there.
        scheduleNotification(asks = false)
        waitForNotification(NOTIFICATION)
        removeFromRecents()
        tapNotification(NOTIFICATION)
        waitFor(Pattern.compile("Search by code or title.*")).click()
        waitForKeyboard()
        device.executeShellCommand("input text plg%snotifications")
        waitFor(Pattern.compile("PLG-025\\s+Local notifications")).click()
        expectCheck(TAP_NAME, containing = "received 1 taps")
        saveScreenshot("notification-launched")
    }

    // Schedules a notification, answers the permission dialog where the app does not hold the permission yet, and waits until the plugin answers with the identifier, which is new for every notification.
    private fun scheduleNotification(asks: Boolean) {
        val before = labelOf(check(SCHEDULED_NAME)) ?: ""
        tap("Notify in 5 seconds")
        if (asks) {
            answerPermission(*ALLOW)
        }
        waitUntil("The plugin does not schedule another notification.") {
            val line = labelOf(check(SCHEDULED_NAME)) ?: ""
            line.isNotEmpty() && line != before && !line.startsWith("Waiting: ")
        }
        expectCheck(SCHEDULED_NAME)
    }
}
