package dev.haylen;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.os.Build;

// Tells the engine whether the device has a network, when the app starts and whenever the default network comes or goes.
final class HaylenNetwork extends ConnectivityManager.NetworkCallback {
    private final ConnectivityManager manager;

    HaylenNetwork(Context context) {
        manager = context.getSystemService(ConnectivityManager.class);
    }

    // The callback only speaks when a default network exists, so the state it starts from is reported first.
    void register() {
        nativeNetwork(manager.getActiveNetwork() != null);
        manager.registerDefaultNetworkCallback(this);
    }

    void unregister() {
        manager.unregisterNetworkCallback(this);
    }

    @Override
    public void onAvailable(Network network) {
        nativeNetwork(true);
    }

    @Override
    public void onLost(Network network) {
        nativeNetwork(false);
    }

    // Called from any thread of the engine, such as a thread of the HTTP client whose request failed, and answers at once whether Android lets the app reach the local network, which an app that targets Android 17 reaches only with the permission `ACCESS_LOCAL_NETWORK`. Without an activity the engine runs nothing that reaches it.
    static boolean isLocalNetworkAllowed() {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN || activity.getApplicationInfo().targetSdkVersion < Build.VERSION_CODES.CINNAMON_BUN) {
            return true;
        }

        return activity.checkSelfPermission(Manifest.permission.ACCESS_LOCAL_NETWORK) == PackageManager.PERMISSION_GRANTED;
    }

    private static native void nativeNetwork(boolean online);
}
