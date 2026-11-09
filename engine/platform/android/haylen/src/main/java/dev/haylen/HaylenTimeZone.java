package dev.haylen;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import androidx.core.content.ContextCompat;
import java.nio.charset.StandardCharsets;
import java.util.TimeZone;

// Tells the engine the time zone of the device, when the activity starts and whenever it changes, while the activity lives.
final class HaylenTimeZone extends BroadcastReceiver {
    private final Context context;

    HaylenTimeZone(Context context) {
        this.context = context;
    }

    void register() {
        ContextCompat.registerReceiver(context, this, new IntentFilter(Intent.ACTION_TIMEZONE_CHANGED), ContextCompat.RECEIVER_NOT_EXPORTED);
        report(TimeZone.getDefault().getID());
    }

    void unregister() {
        context.unregisterReceiver(this);
    }

    // The broadcast names the new zone, since the default zone of the process may change only after the receivers ran.
    @Override
    public void onReceive(Context receiver, Intent intent) {
        report(intent.getStringExtra(Intent.EXTRA_TIMEZONE));
    }

    private static void report(String zone) {
        nativeTimeZone(zone.getBytes(StandardCharsets.UTF_8));
    }

    private static native void nativeTimeZone(byte[] zone);
}
