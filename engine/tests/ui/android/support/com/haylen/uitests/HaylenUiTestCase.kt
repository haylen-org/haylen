package com.haylen.uitests

import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Rect
import android.location.Location
import android.location.LocationManager
import android.location.provider.ProviderProperties
import android.os.SystemClock
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.BySelector
import androidx.test.uiautomator.Configurator
import androidx.test.uiautomator.StaleObjectException
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject2
import androidx.test.uiautomator.Until
import java.io.File
import java.util.regex.Pattern
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before

// The base of the UI tests of a Haylen app on Android. The command "haylen test --ui" installs the app and names it to the instrumentation of the runner, and each test starts it again with the launch arguments of its class. The GUIs of the app expose their components to the tests through the accessibility tree, each as a node whose content description is the text it shows, a text field as a node whose hint is its label and whose text is its value, and native views show their text.
abstract class HaylenUiTestCase {
    companion object {
        const val TIMEOUT = 60_000L
        const val POLL = 250L
        // The extra of the launch intent that the engine reads its launch arguments from.
        const val ARGUMENTS_EXTRA = "dev.haylen.arguments"
        const val PERMISSION_CONTROLLER = "com.android.permissioncontroller"
        // How often a simulated location reaches the system, the way a receiver reports its fixes.
        const val LOCATION_INTERVAL = 500L
        // The longest that UI Automator waits for the screen to settle before a search or a tap.
        const val IDLE_TIMEOUT = 1_000L
    }

    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val device: UiDevice = UiDevice.getInstance(instrumentation)

    init {
        // UI Automator waits for the screen to settle before each search and each tap, which lets the animation of a dialog of the system end before a tap reaches it. The engine draws every frame and its accessibility tree follows live values, such as a count of camera frames, so the app never settles, and the wait stays short while the tests wait for what they check.
        Configurator.getInstance().waitForIdleTimeout = IDLE_TIMEOUT
    }
    // The meta-data of the test APK names the app it drives, which "haylen test --ui" writes into it.
    val identifier: String = instrumentation.context.packageManager.getApplicationInfo(instrumentation.context.packageName, PackageManager.GET_META_DATA).metaData?.getString("haylenUiApp")
        ?: error("Build the UI tests with \"haylen test --ui\", which names the app they drive.")

    // The launch arguments of the app, which it reads through "haylen.launchArguments". A test class overrides it to start the app somewhere, such as on one screen.
    open val launchArguments: List<String> = emptyList()

    // The runtime permissions that the app loses before it starts, such as "android.permission.CAMERA", so their dialogs show again whatever a test before asked.
    open val revokedPermissions: List<String> = emptyList()

    @Before
    fun launch() {
        device.executeShellCommand("am force-stop $identifier")
        for (permission in revokedPermissions) {
            device.executeShellCommand("pm revoke $identifier $permission")
            device.executeShellCommand("pm clear-permission-flags $identifier $permission user-set user-fixed")
        }
        launchApp(launchArguments)
    }

    private var simulatedLocation: Thread? = null

    @After
    fun stop() {
        device.executeShellCommand("am force-stop $identifier")
        stopSimulatingLocation()
    }

    // Reports the place as the fixes of the GPS and the network providers until the test ends, through test providers that the operation "android:mock_location" lets the runner add.
    fun simulateLocation(latitude: Double, longitude: Double) {
        device.executeShellCommand("appops set ${instrumentation.context.packageName} android:mock_location allow")
        val locations = instrumentation.context.getSystemService(LocationManager::class.java)
        val providers = listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER)
        for (provider in providers) {
            locations.addTestProvider(provider, false, false, false, false, true, true, true, ProviderProperties.POWER_USAGE_LOW, ProviderProperties.ACCURACY_FINE)
            locations.setTestProviderEnabled(provider, true)
        }
        simulatedLocation = Thread {
            while (!Thread.currentThread().isInterrupted) {
                for (provider in providers) {
                    locations.setTestProviderLocation(provider, fix(provider, latitude, longitude))
                }
                try {
                    Thread.sleep(LOCATION_INTERVAL)
                } catch (interrupted: InterruptedException) {
                    return@Thread
                }
            }
        }.apply { start() }
    }

    private fun fix(provider: String, latitude: Double, longitude: Double): Location {
        val location = Location(provider)
        location.latitude = latitude
        location.longitude = longitude
        location.accuracy = 1f
        location.time = System.currentTimeMillis()
        location.elapsedRealtimeNanos = SystemClock.elapsedRealtimeNanos()
        return location
    }

    private fun stopSimulatingLocation() {
        val thread = simulatedLocation ?: return
        thread.interrupt()
        thread.join()
        simulatedLocation = null
        val locations = instrumentation.context.getSystemService(LocationManager::class.java)
        for (provider in listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER)) {
            locations.removeTestProvider(provider)
        }
    }

    // Starts the app from its launcher intent with the launch arguments and waits until its window shows.
    fun launchApp(arguments: List<String>) {
        val context = instrumentation.context
        val intent = context.packageManager.getLaunchIntentForPackage(identifier) ?: error("The app \"$identifier\" is not installed.")
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        if (arguments.isNotEmpty()) {
            intent.putExtra(ARGUMENTS_EXTRA, arguments.toTypedArray())
        }
        context.startActivity(intent)
        waitForApp()
    }

    fun waitForApp() {
        assertTrue("The app \"$identifier\" does not show.", device.wait(Until.hasObject(By.pkg(identifier).depth(0)), TIMEOUT))
    }

    // The node whose content description or text is the label, or the text field whose hint it is, the first that the screen shows.
    fun element(label: String): UiObject2? = device.findObject(By.desc(label)) ?: device.findObject(By.text(label)) ?: device.findObject(By.hint(label))

    // The node whose content description or text starts with the text, such as a line of a log that ends with a value the test does not know.
    fun elementStartingWith(prefix: String): UiObject2? = device.findObject(By.descStartsWith(prefix)) ?: device.findObject(By.textStartsWith(prefix)) ?: device.findObject(By.hintStartsWith(prefix))

    // The content description of the node, or the text of a native view, or null without a node or once the app built its accessibility tree again after the search found the node, which a later search finds anew.
    fun labelOf(node: UiObject2?): String? {
        return try {
            node?.contentDescription ?: node?.text
        } catch (stale: StaleObjectException) {
            null
        }
    }

    // Waits until the screen shows a node whose label starts with the text, and returns the label.
    fun waitForStart(prefix: String, timeout: Long = TIMEOUT): String {
        var found: String? = null
        waitUntil("The screen shows nothing that starts with \"$prefix\" after ${timeout / 1000} seconds.", timeout) {
            found = labelOf(elementStartingWith(prefix))
            found != null
        }
        return found!!
    }

    // The node whose content description or text matches the regular expression, or the text field whose hint does.
    fun element(pattern: Pattern): UiObject2? = device.findObject(By.desc(pattern)) ?: device.findObject(By.text(pattern)) ?: device.findObject(By.hint(pattern))

    fun waitFor(label: String, timeout: Long = TIMEOUT): UiObject2 {
        var found: UiObject2? = null
        waitUntil("The screen shows no \"$label\" after ${timeout / 1000} seconds.", timeout) {
            found = element(label)
            found != null
        }
        return found!!
    }

    fun waitFor(pattern: Pattern, timeout: Long = TIMEOUT): UiObject2 {
        var found: UiObject2? = null
        waitUntil("The screen shows nothing that matches \"${pattern.pattern()}\" after ${timeout / 1000} seconds.", timeout) {
            found = element(pattern)
            found != null
        }
        return found!!
    }

    fun waitFor(selector: BySelector, description: String, timeout: Long = TIMEOUT): UiObject2 {
        return device.wait(Until.findObject(selector), timeout) ?: throw AssertionError("The screen shows no $description after ${timeout / 1000} seconds.")
    }

    fun tap(label: String) {
        waitFor(label).click()
    }

    // Taps the native button with the text, which the theme of Android may show in capitals, such as the buttons of a dialog.
    fun tapButton(label: String) {
        waitFor(Pattern.compile(Pattern.quote(label), Pattern.CASE_INSENSITIVE)).click()
    }

    // Checks the condition four times a second until it holds or the time ends, and returns whether it held.
    fun poll(timeout: Long = TIMEOUT, condition: () -> Boolean): Boolean {
        val deadline = System.currentTimeMillis() + timeout
        while (!condition()) {
            if (System.currentTimeMillis() > deadline) {
                return false
            }

            Thread.sleep(POLL)
        }
        return true
    }

    // Waits until the condition holds and fails with the message when it never does.
    fun waitUntil(message: String, timeout: Long = TIMEOUT, condition: () -> Boolean) {
        if (!poll(timeout, condition)) {
            fail(message)
        }
    }

    // Taps the button of the permission dialog of the system with one of the resource ids of the permission controller, such as "permission_allow_foreground_only_button", "permission_allow_button" or "permission_deny_button". The dialog names a button by the state of the permission, such as "permission_deny_and_dont_ask_again_button" for a refusal that the person made before, so a test names every id of the answer.
    fun answerPermission(vararg buttons: String) {
        val ids = Pattern.compile("$PERMISSION_CONTROLLER:id/(${buttons.joinToString("|")})")
        val answer = waitFor(By.res(ids), "a button ${buttons.joinToString(" or ") { "\"$it\"" }} of a permission dialog")
        answer.click()
        assertTrue("The permission dialog stays after the tap.", device.wait(Until.gone(By.res(ids)), TIMEOUT))
    }

    // Opens the notification shade and taps the notification whose text holds the words, which brings the app to the front or launches it.
    fun tapNotification(text: String) {
        device.openNotification()
        waitFor(By.textContains(text), "a notification with \"$text\"").click()
        waitForApp()
    }

    // Whether the tests run on the Android emulator, which sets the property "ro.boot.qemu", and which lacks hardware such as a microphone that hears a room.
    fun isEmulator(): Boolean = device.executeShellCommand("getprop ro.boot.qemu").trim() == "1"

    // Leaves the app for the home screen, which sends it to the background.
    fun goHome() {
        device.pressHome()
        assertTrue("The app stays in front after Home.", device.wait(Until.gone(By.pkg(identifier).depth(0)), TIMEOUT))
    }

    // Removes the task of the app from the recent apps, which the system does when a person swipes its card away, whatever the launcher draws, and ends the process once it holds no activity, as the system does when it needs the memory, so the next launch starts a new process. The alarms and notifications of the app stay, which a force stop would cancel.
    fun removeFromRecents() {
        device.pressHome()
        val recents = device.executeShellCommand("dumpsys activity recents")
        val task = Regex("Recent #\\d+: Task\\{\\w+ #(\\d+) type=standard A=\\d+:${Regex.escape(identifier)}\\}").find(recents)?.groupValues?.get(1)
            ?: throw AssertionError("The recent apps hold no task of \"$identifier\".")
        device.executeShellCommand("am stack remove $task")
        waitUntil("The activity of the app stays after its task left the recent apps.") {
            device.executeShellCommand("dumpsys activity processes $identifier").contains("(cch-empty)")
        }
        device.executeShellCommand("am kill $identifier")
        waitUntil("The process of the app stays after its task left the recent apps.") {
            device.executeShellCommand("pidof $identifier").isBlank()
        }
    }

    // Saves a picture of the window of the app, never the whole screen, as a PNG named after the text in the folder "ui-tests" of the files of the runner, which the run copies back.
    fun saveScreenshot(name: String) {
        val window = waitFor(By.pkg(identifier).depth(0), "the window of the app")
        val bounds: Rect = window.visibleBounds
        val screen = instrumentation.uiAutomation.takeScreenshot() ?: error("The system took no screenshot.")
        val picture = Bitmap.createBitmap(screen, bounds.left, bounds.top, bounds.width(), bounds.height())
        val folder = File(instrumentation.context.filesDir, "ui-tests")
        folder.mkdirs()
        File(folder, "$name.png").outputStream().use { picture.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }
}
