package com.haylen.uitests

import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.CAMERA
import android.Manifest.permission.POST_NOTIFICATIONS
import android.Manifest.permission.RECORD_AUDIO
import java.util.regex.Pattern
import org.junit.Test

// PLG-011: the permission dialogs of the camera, the microphone, the location and notifications answer the app, the camera of the plugin follows the answer, and the tap on a notification reaches the app while it shows, from the home screen and after the app left the recent apps. The test takes the permissions back before it starts, so every dialog shows.
class PermissionTests : TestProjectCase() {
    companion object {
        val ALLOW_WHILE_USING = arrayOf("permission_allow_foreground_only_button")
        val ALLOW = arrayOf("permission_allow_button")
        val DENY = arrayOf("permission_deny_button", "permission_deny_and_dont_ask_again_button")
        const val NOTIFICATION = "Tap to open Haylen Tests."
        const val TAP_NAME = "The tap on the notification reaches the app"
    }

    override val code = "PLG-011"
    override val revokedPermissions = listOf(CAMERA, RECORD_AUDIO, ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION, POST_NOTIFICATIONS)

    @Test
    fun thePermissionDialogsAnswerTheAppAndTheNotificationTapsReachIt() {
        ask("camera", "Ask for the camera", ALLOW_WHILE_USING, "granted")
        ask("microphone", "Ask for the microphone", DENY, "denied")
        ask("location", "Ask for the location", ALLOW_WHILE_USING, "granted")
        ask("notifications", "Ask for notifications", ALLOW, "granted")
        tap("Start the camera")
        expectCheck("The camera of the plugin follows the permission", containing = "The camera started at")
        saveScreenshot("permissions")

        scheduleNotification()
        tapNotification(NOTIFICATION)
        expectCheck(TAP_NAME, containing = "received 1 taps")

        scheduleNotification()
        goHome()
        tapNotification(NOTIFICATION)
        expectCheck(TAP_NAME, containing = "received 2 taps")

        // The system starts the app without launch arguments, so the test opens again through the search of the menu.
        scheduleNotification()
        removeFromRecents()
        tapNotification(NOTIFICATION)
        waitFor(Pattern.compile("Search by code or title.*")).click()
        device.executeShellCommand("input text plg%spermissions")
        waitFor(Pattern.compile("PLG-011\\s+Permissions")).click()
        expectCheck(TAP_NAME, containing = "received 1 taps")
        saveScreenshot("notification-launched")
    }

    private fun ask(kind: String, button: String, answer: Array<String>, state: String) {
        tap(button)
        answerPermission(*answer)
        expectCheck("The $kind permission reaches the app", containing = "The request answered \"$state\".")
    }

    // Schedules a notification and waits until the plugin answers with its identifier, which is new for every notification.
    private fun scheduleNotification() {
        val before = labelOf(check("A local notification is scheduled")) ?: ""
        tap("Notify in 5 seconds")
        waitUntil("The plugin does not schedule another notification.") {
            val line = labelOf(check("A local notification is scheduled")) ?: ""
            line.isNotEmpty() && line != before
        }
        expectCheck("A local notification is scheduled")
    }
}
