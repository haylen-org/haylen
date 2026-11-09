package com.haylen.plugins.nativedemo

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import dev.haylen.HaylenPermission
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenRequirements

// The permissions that the demo owns on Android, which the engine asks the person for through the permissions of the manifest of the plugin: the camera, the microphone, the location while the app is in use and the notifications.
object NativeDemoPermissions {
    fun register(context: HaylenPluginContext) {
        context.registerPermission(Device("camera", "the camera", Manifest.permission.CAMERA, PackageManager.FEATURE_CAMERA_ANY, "This device has no camera."))
        context.registerPermission(Device("microphone", "the microphone", Manifest.permission.RECORD_AUDIO, PackageManager.FEATURE_MICROPHONE, "This device has no microphone."))
        context.registerPermission(Location())
        context.registerPermission(Notifications())
    }

    // A device without the hardware, such as a TV without a camera or a microphone, answers unsupported with the reason instead of a prompt.
    private class Device(name: String, capability: String, permission: String, private val feature: String, private val lacking: String) : HaylenPermission(name, capability, permission) {
        override fun unsupportedReason(context: Context): String? {
            return if (context.packageManager.hasSystemFeature(feature)) null else lacking
        }
    }

    // The prompt offers the precise location next to the approximate one where the manifest declares it, and either one counts as granted.
    private class Location : HaylenPermission("location", "the location", Manifest.permission.ACCESS_COARSE_LOCATION) {
        override fun requested(context: Context, requirements: HaylenRequirements): Array<String> {
            val precise = requirements.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION)
            return if (precise) arrayOf(Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION) else required(context)
        }

        override fun isGranted(context: Context): Boolean {
            val approximate = ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
            return approximate || ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }
    }

    // Before Android 13 notifications need no permission, while the person may still turn them off in the settings, which no prompt turns on again.
    private class Notifications : HaylenPermission("notifications", "notifications", Manifest.permission.POST_NOTIFICATIONS) {
        override fun required(context: Context): Array<String> {
            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) super.required(context) else emptyArray()
        }

        override fun stateWithoutPrompt(context: Context): String {
            return if (NotificationManagerCompat.from(context).areNotificationsEnabled()) "granted" else "denied"
        }
    }
}
