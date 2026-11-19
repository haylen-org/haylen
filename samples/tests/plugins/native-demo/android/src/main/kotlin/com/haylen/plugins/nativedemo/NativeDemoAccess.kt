package com.haylen.plugins.nativedemo

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenRequirements
import dev.haylen.HaylenScreen
import org.json.JSONObject

// The capabilities of the demo that Android gates behind permissions of the manifest: the camera, the microphone, the location while the app is in use and the notifications. Each one has the screen `<name>Permission`, whose Activity Result contract shows the permission dialog of the system under the stable key `haylen.native-demo.<name>Permission`, and which the Lua API opens before it starts the capability. Android answers at once without a dialog where the app holds the permissions already. The handlers check the capability again before they use it, since the person may take a permission back in the settings at any time.
class NativeDemoAccess(private val context: HaylenPluginContext) {
    // The hardware feature that a capability needs, with the sentence that tells a device without it.
    private class Hardware(val feature: String, val absent: String)

    // A capability with the hardware that it needs, the permissions of the manifest that it needs on the Android that runs, the ones that the dialog offers too where the manifest declares them, how the plugin tells that the app holds it and what a refusal says.
    private class Capability(val name: String, val hardware: Hardware?, val required: Array<String>, val optional: Array<String>, val held: (Context) -> Boolean, val refusal: String)

    private val capabilities = listOf(
        Capability(
            name = "camera",
            hardware = Hardware(PackageManager.FEATURE_CAMERA_ANY, "This device has no camera."),
            required = arrayOf(Manifest.permission.CAMERA),
            optional = emptyArray(),
            held = { granted(it, Manifest.permission.CAMERA) },
            refusal = REFUSAL.format("the camera"),
        ),
        Capability(
            name = "microphone",
            hardware = Hardware(PackageManager.FEATURE_MICROPHONE, "This device has no microphone."),
            required = arrayOf(Manifest.permission.RECORD_AUDIO),
            optional = emptyArray(),
            held = { granted(it, Manifest.permission.RECORD_AUDIO) },
            refusal = REFUSAL.format("the microphone"),
        ),
        // The dialog offers the precise location next to the approximate one where the manifest declares it, and either one counts.
        Capability(
            name = "location",
            hardware = null,
            required = arrayOf(Manifest.permission.ACCESS_COARSE_LOCATION),
            optional = arrayOf(Manifest.permission.ACCESS_FINE_LOCATION),
            held = { granted(it, Manifest.permission.ACCESS_COARSE_LOCATION) || granted(it, Manifest.permission.ACCESS_FINE_LOCATION) },
            refusal = REFUSAL.format("the location"),
        ),
        // Before Android 13 notifications need no permission, while the person may still turn them off in the settings, which no dialog turns on again.
        Capability(
            name = "notifications",
            hardware = null,
            required = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) arrayOf(Manifest.permission.POST_NOTIFICATIONS) else emptyArray(),
            optional = emptyArray(),
            held = { NotificationManagerCompat.from(it).areNotificationsEnabled() },
            refusal = "The person has not allowed the notifications of the app, or turned them off in its settings, where the person turns them on again.",
        ),
    )

    fun register() {
        for (capability in capabilities) {
            context.registerScreen(capability.name + "Permission", ActivityResultContracts.RequestMultiplePermissions(), HaylenScreen.Input<Array<String>> { requested(capability) }, HaylenScreen.Output<Map<String, Boolean>> { answer(capability) })
        }
    }

    // Fails with the code `unsupported` where the device or the project lacks what the capability needs, and with the code `denied` where the person has not allowed it.
    fun check(name: String) {
        val capability = capabilities.first { it.name == name }
        requireSupported(capability)
        if (!capability.held(context.application())) {
            throw HaylenBridge.Failure(capability.refusal, "denied", null)
        }
    }

    // The permissions that the dialog asks for, which are none where the app holds the capability already, so Android answers without a dialog.
    private fun requested(capability: Capability): Array<String> {
        requireSupported(capability)
        if (capability.held(context.application())) {
            return emptyArray()
        }
        val requirements = context.requirements()
        val declared = capability.required + capability.optional.filter { requirements.hasPermission(it) }
        return declared.filter { !granted(context.application(), it) }.toTypedArray()
    }

    // The answer of the dialog counts only through the state that the app holds after it, which also covers the notifications that the settings turned off.
    private fun answer(capability: Capability): JSONObject {
        check(capability.name)
        return JSONObject().put("capability", capability.name).put("granted", true).put("language", LANGUAGE)
    }

    private fun requireSupported(capability: Capability) {
        val hardware = capability.hardware
        if (hardware != null && !context.application().packageManager.hasSystemFeature(hardware.feature)) {
            throw HaylenBridge.Failure(hardware.absent, "unsupported", null)
        }
        val requirements = capability.required.map { HaylenRequirements.Requirement.permission(it) }
        context.requirements().require(*requirements.toTypedArray())
    }

    private companion object {
        const val LANGUAGE = "Kotlin"
        const val REFUSAL = "The person has not allowed %s. Android shows its dialog again until the person refuses twice, and after that only the settings of the app allow it."

        fun granted(context: Context, permission: String): Boolean = ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
    }
}
