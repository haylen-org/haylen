package dev.haylen;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;

// Receives the links and the notification taps that open the app, whose intent filters plugins declare on this activity. It hands them to the running HaylenActivity and brings the task of the app to the front as the launcher icon does, with whatever screen shows over the app, or starts HaylenActivity with them when none runs. It shows nothing and finishes at once, in a task of its own.
public final class HaylenLinkActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent link = new Intent(getIntent()).setClass(this, HaylenActivity.class);
        // The task of the app keeps the launcher intent, since a launch whose intent differs from the one the task started with adds a second activity over a screen that shows.
        Intent launch = Intent.makeMainActivity(new ComponentName(this, HaylenActivity.class)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        HaylenActivity running = HaylenBridge.activity();
        if (running != null) {
            running.receiveIntent(link);
        } else {
            launch.putExtra(HaylenActivity.EXTRA_LINK, link);
        }
        startActivity(launch);
        finish();
    }
}
