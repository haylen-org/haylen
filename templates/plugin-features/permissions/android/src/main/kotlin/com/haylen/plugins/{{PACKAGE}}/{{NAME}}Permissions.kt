package com.haylen.plugins.{{PACKAGE}}

import android.content.pm.PackageManager
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import dev.haylen.HaylenActivity
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenRequirements

// Asks the person for the permissions of the {{TITLE}} plugin through the Activity Result API, after it checked that the manifest of the app declares them. Each activity registers the launcher before it starts, under a key of the plugin that stays the same for every activity, and one request shows at a time.
class {{NAME}}Permissions {
    private var launcher: ActivityResultLauncher<String>? = null
    private var asking: ((Boolean) -> Unit)? = null

    fun register(activity: HaylenActivity) {
        launcher = activity.activityResultRegistry.register("{{ID}}.permission", activity, ActivityResultContracts.RequestPermission(), ::answered)
    }

    // The activity takes its launcher with it.
    fun unregister() {
        launcher = null
    }

    // Runs `granted` once the app holds the permission, asking the person when it does not yet, and fails the call when the manifest lacks the permission, the person refuses it or `granted` throws.
    fun withPermission(context: HaylenPluginContext, permission: String, reply: HaylenBridge.Reply, granted: () -> Unit) {
        context.requirements().require(HaylenRequirements.Requirement.permission(permission))
        if (ContextCompat.checkSelfPermission(context.application(), permission) == PackageManager.PERMISSION_GRANTED) {
            granted()
            return
        }
        val current = launcher ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
        if (asking != null) {
            throw HaylenBridge.Failure("Another permission request shows.", "busy", null)
        }
        asking = { allowed ->
            try {
                if (allowed) granted() else reply.failure("The person has not allowed \"$permission\".", "permissionDenied", null)
            } catch (error: Exception) {
                reply.failure(error)
            }
        }
        current.launch(permission)
    }

    // A request that ends after the process ended reaches the launcher of the new activity, while no call waits for it.
    private fun answered(granted: Boolean) {
        val waiting = asking ?: return
        asking = null
        waiting(granted)
    }
}
