package dev.haylen;

import android.content.Context;
import android.os.Build;
import android.os.PowerManager;

// Tells the engine the thermal status of the device that the power manager reports from Android 10 on, when the activity starts and whenever it changes, while the activity lives. Older versions of Android report none, so the engine keeps the state unknown there.
final class HaylenThermalState implements PowerManager.OnThermalStatusChangedListener {
    private final Context context;

    HaylenThermalState(Context context) {
        this.context = context;
    }

    void register() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return;
        }

        PowerManager power = context.getSystemService(PowerManager.class);
        power.addThermalStatusListener(context.getMainExecutor(), this);
        nativeThermalState(power.getCurrentThermalStatus());
    }

    void unregister() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return;
        }

        context.getSystemService(PowerManager.class).removeThermalStatusListener(this);
    }

    @Override
    public void onThermalStatusChanged(int status) {
        nativeThermalState(status);
    }

    private static native void nativeThermalState(int status);
}
