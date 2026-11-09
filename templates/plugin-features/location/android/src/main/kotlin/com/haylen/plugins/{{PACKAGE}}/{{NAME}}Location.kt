package com.haylen.plugins.{{PACKAGE}}

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.CancellationSignal
import android.os.Looper
import androidx.core.content.ContextCompat
import androidx.core.location.LocationManagerCompat
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPermission
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenRequirements
import org.json.JSONObject

// The location of the {{TITLE}} plugin on Android: one fix of `LocationManager`, from the GPS where it is on and the person allowed the precise location, and from the network otherwise, or every change while the app watches, sent batched as the event `located`, after the person allowed the location.
class {{NAME}}Location(private val context: HaylenPluginContext) {
    private var watcher: LocationListener? = null

    fun register() {
        context.registerPermission(LocationPermission())

        context.register("locate") { _, reply ->
            context.requirePermission("location", reply) { locate(reply) }
        }

        context.register("watchLocation") { _, reply ->
            context.requirePermission("location", reply) {
                watch()
                reply.success(null)
            }
        }

        context.register("stopWatchingLocation") { _, reply ->
            stopWatching()
            reply.success(null)
        }
    }

    // The GPS reads the location only for an app that holds the precise one, while a person who allowed the approximate one gets the location of the network.
    private fun manager(): Pair<LocationManager, String> {
        val locations = context.application().getSystemService(LocationManager::class.java)
        val precise = ContextCompat.checkSelfPermission(context.application(), Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        val providers = if (precise) listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER) else listOf(LocationManager.NETWORK_PROVIDER)
        val provider = providers.firstOrNull { locations?.isProviderEnabled(it) == true }
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

// The permission `location` that the plugin owns, the location while the app is in use at the precision the person allows: the prompt offers the precise location next to the approximate one where the manifest declares it, and either one counts as granted.
private class LocationPermission : HaylenPermission("location", "the location", Manifest.permission.ACCESS_COARSE_LOCATION) {
    override fun requested(context: Context, requirements: HaylenRequirements): Array<String> {
        val precise = requirements.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION)
        return if (precise) arrayOf(Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION) else required(context)
    }

    override fun isGranted(context: Context): Boolean {
        val approximate = ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
        return approximate || ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
    }
}
