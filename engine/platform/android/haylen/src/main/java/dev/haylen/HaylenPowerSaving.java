package dev.haylen;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.PowerManager;
import androidx.core.content.ContextCompat;

// Tells the engine whether the battery saver of Android is on, when the activity starts and whenever it changes, while the activity lives.
final class HaylenPowerSaving extends BroadcastReceiver {
    private final Context context;

    HaylenPowerSaving(Context context) {
        this.context = context;
    }

    void register() {
        ContextCompat.registerReceiver(context, this, new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED), ContextCompat.RECEIVER_NOT_EXPORTED);
        report();
    }

    void unregister() {
        context.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context receiver, Intent intent) {
        report();
    }

    private void report() {
        nativePowerSaving(context.getSystemService(PowerManager.class).isPowerSaveMode());
    }

    private static native void nativePowerSaving(boolean enabled);
}
