package dev.haylen;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import androidx.core.content.ContextCompat;

// Tells the engine the battery of the device from the sticky `ACTION_BATTERY_CHANGED` broadcast, when the activity starts and whenever the battery changes, while the activity lives.
final class HaylenBattery extends BroadcastReceiver {
    private final Context context;

    HaylenBattery(Context context) {
        this.context = context;
    }

    // The broadcast is sticky, so registering returns the battery as it is now, which is reported first.
    void register() {
        Intent current = ContextCompat.registerReceiver(context, this, new IntentFilter(Intent.ACTION_BATTERY_CHANGED), ContextCompat.RECEIVER_NOT_EXPORTED);
        if (current != null) {
            onReceive(context, current);
        }
    }

    void unregister() {
        context.unregisterReceiver(this);
    }

    // A device without a battery, such as a TV, leaves the battery out of the broadcast.
    @Override
    public void onReceive(Context receiver, Intent intent) {
        nativeBattery(intent.getBooleanExtra(BatteryManager.EXTRA_PRESENT, false), intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1), intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1), intent.getIntExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_UNKNOWN));
    }

    private static native void nativeBattery(boolean present, int level, int scale, int status);
}
