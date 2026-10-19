package com.haylen.plugins.{{PACKAGE}}

import android.Manifest
import android.annotation.SuppressLint
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.CancellationSignal
import android.os.Looper
import androidx.core.content.ContextCompat
import androidx.core.location.LocationManagerCompat
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPluginContext
import org.json.JSONObject

// The location of the {{TITLE}} plugin on Android: one fix of `LocationManager`, from the GPS where it is on and from the network otherwise, or every change while the app watches, sent batched as the event `located`, after the person allowed the location.
class {{NAME}}Location(private val context: HaylenPluginContext, private val permissions: {{NAME}}Permissions) {
    private var watcher: LocationListener? = null

    fun register() {
        context.register("locate") { _, reply ->
            permissions.withPermission(context, Manifest.permission.ACCESS_FINE_LOCATION, reply) { locate(reply) }
        }

        context.register("watchLocation") { _, reply ->
            permissions.withPermission(context, Manifest.permission.ACCESS_FINE_LOCATION, reply) {
                watch()
                reply.success(null)
            }
        }

        context.register("stopWatchingLocation") { _, reply ->
            stopWatching()
            reply.success(null)
        }
    }

    private fun manager(): Pair<LocationManager, String> {
        val locations = context.application().getSystemService(LocationManager::class.java)
        val provider = listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER).firstOrNull { locations?.isProviderEnabled(it) == true }
        if (locations == null || provider == null) {
            throw HaylenBridge.Failure("This device has no location turned on.", "unsupported", null)
        }
        return locations to provider
    }

    @SuppressLint("MissingPermission")
    private fun locate(reply: HaylenBridge.Reply) {
        val (locations, provider) = manager()
        val cancel = CancellationSignal()
        reply.onCancel { cancel.cancel() }
        LocationManagerCompat.getCurrentLocation(locations, provider, cancel, ContextCompat.getMainExecutor(context.application())) { location: Location? ->
            if (location == null) {
                reply.failure("The location is unknown.", "locationUnknown", null)
            } else {
                reply.success(place(location))
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun watch() {
        val (locations, provider) = manager()
        stopWatching()
        val listener = LocationListener { location -> context.emit("located", place(location), false, true) }
        locations.requestLocationUpdates(provider, 1000L, 0f, listener, Looper.getMainLooper())
        watcher = listener
    }

    private fun stopWatching() {
        val listener = watcher ?: return
        context.application().getSystemService(LocationManager::class.java)?.removeUpdates(listener)
        watcher = null
    }

    private fun place(location: Location): JSONObject = JSONObject().put("latitude", location.latitude).put("longitude", location.longitude).put("accuracy", location.accuracy.toDouble())
}
