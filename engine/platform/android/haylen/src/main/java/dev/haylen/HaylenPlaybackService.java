package dev.haylen;

import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.IBinder;
import androidx.core.app.ServiceCompat;

// Keeps an app that plays audio in the background in the foreground while what it plays shows as playing, with the media notification of its media session as the notification of the service, so Android neither freezes nor ends the process while the sound plays. The library `haylen-playback` declares it with the type `mediaPlayback` and its permissions, and `haylen.py` adds the library to the apps whose `app.json` sets `audio.background`.
public final class HaylenPlaybackService extends Service {
    // Android expects the notification of the service at once, also when the app paused in between, so the service shows it and then leaves the foreground again.
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        ServiceCompat.startForeground(this, HaylenMediaSession.NOTIFICATION, HaylenMediaSession.notification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        if (!HaylenMediaSession.isPlayingInBackground()) {
            stopSelf();
        }
        return START_NOT_STICKY;
    }

    // The notification stays in the shade with the paused controls, and goes with the service once the app left the controls, since Android keeps the notification of a service from being cancelled.
    @Override
    public void onDestroy() {
        ServiceCompat.stopForeground(this, HaylenMediaSession.isShowing() ? ServiceCompat.STOP_FOREGROUND_DETACH : ServiceCompat.STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
