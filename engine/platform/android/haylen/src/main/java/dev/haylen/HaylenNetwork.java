package dev.haylen;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;

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

    private static native void nativeNetwork(boolean online);
}
