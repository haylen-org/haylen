package com.haylen.uitests

import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.CAMERA
import android.Manifest.permission.RECORD_AUDIO
import java.util.Locale
import java.util.regex.Pattern
import org.junit.Test

// PLG-016: the back and the front cameras fill the video stream, a photo arrives as JPEG bytes that decode into a texture, and the microphone fills its stream with its level, after the permission dialogs.
class CameraTests : TestProjectCase() {
    override val code = "PLG-016"
    override val revokedPermissions = listOf(CAMERA, RECORD_AUDIO)

    @Test
    fun theCamerasThePhotoAndTheMicrophoneReachTheApp() {
        tap("Start the back camera")
        answerPermission(*PermissionTests.ALLOW_WHILE_USING)
        expectCheck("The frames of the camera reach the texture", containing = "of the back camera")
        tap("Take a photo")
        expectCheck("A photo arrives as JPEG bytes", containing = "decoded into a texture")
        saveScreenshot("back-camera")
        tap("Stop the camera")
        tap("Start the front camera")
        expectCheck("The frames of the camera reach the texture", containing = "of the front camera")
        saveScreenshot("front-camera")
        tap("Stop the camera")

        tap("Start the microphone")
        answerPermission(*PermissionTests.ALLOW_WHILE_USING)
        expectCheck("The microphone fills its stream and reports its level", containing = "level reports arrived")
        saveScreenshot("microphone")
    }
}

// PLG-017: after the permission dialog, the plugin tells where the device is, which the runner simulates through test providers of the location, and shows a native map over the app at that place.
class LocationTests : TestProjectCase() {
    companion object {
        const val LATITUDE = 48.85837
        const val LONGITUDE = 2.29448
    }

    override val code = "PLG-017"
    override val revokedPermissions = listOf(ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION)

    @Test
    fun thePlaceAndTheMapReachTheApp() {
        simulateLocation(LATITUDE, LONGITUDE)
        tap("Find the location")
        answerPermission(*PermissionTests.ALLOW_WHILE_USING)
        expectCheck("The platform tells where the device is", containing = String.format(Locale.ROOT, "found %.5f, %.5f", LATITUDE, LONGITUDE))
        tap("Show the map")
        expectCheck("A native map shows over the app", containing = "placed a map drawn with")
        saveScreenshot("map")
    }
}

// The back button of Android leaves a test for the list of its category, whose title shows.
class BackTests : TestProjectCase() {
    override val code = "PLG-015"

    @Test
    fun backLeavesTheTestForItsList() {
        // The activity takes back once the app asked for it, which reaches the main thread after the first frames, so the press waits until the screen settled.
        device.waitForIdle(TIMEOUT)
        device.pressBack()
        waitFor(Pattern.compile("PLG\\s+Plugins,.*", Pattern.DOTALL))
        saveScreenshot("back")
    }
}
