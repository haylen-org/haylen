package dev.haylen;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;
import android.view.KeyEvent;
import java.nio.charset.StandardCharsets;
import org.json.JSONException;
import org.json.JSONObject;

// The now-playing controls of Android: a media session, which headsets, Bluetooth devices and TV remotes send their media buttons to, and the media notification that shows it in the shade and on the lock screen, where Android 13 and later draw its controls from the playback state. Notifications of a media session need no permission. The commands reach the engine with the order of `MediaSession::Command`.
final class HaylenMediaSession extends MediaSession.Callback {
    private static final String CHANNEL = "haylen.media";
    private static final int NOTIFICATION = 0x4d454449;
    private static final long[] ACTIONS = {PlaybackState.ACTION_PLAY, PlaybackState.ACTION_PAUSE, PlaybackState.ACTION_PLAY_PAUSE, PlaybackState.ACTION_STOP, PlaybackState.ACTION_SKIP_TO_NEXT, PlaybackState.ACTION_SKIP_TO_PREVIOUS, PlaybackState.ACTION_SEEK_TO};
    private static final int PLAY = 0;
    private static final int PAUSE = 1;
    private static final int TOGGLE = 2;
    private static final int STOP = 3;
    private static final int NEXT = 4;
    private static final int PREVIOUS = 5;
    private static final int SEEK = 6;

    private static HaylenMediaSession current;

    private final HaylenActivity activity;
    private final MediaSession session;

    private HaylenMediaSession(HaylenActivity owner) {
        activity = owner;
        session = new MediaSession(owner, "haylen");
        session.setCallback(this);
    }

    // Called from the frame thread of the engine with the JSON of `{title, artist, album, duration, position, playing, commands}` and the bytes of the artwork, or with no JSON to leave the controls.
    static void update(byte[] json, byte[] artwork) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> {
            if (json == null) {
                release();
                return;
            }
            if (current == null || current.activity != activity) {
                release();
                current = new HaylenMediaSession(activity);
            }
            try {
                current.show(new JSONObject(new String(json, StandardCharsets.UTF_8)), artwork);
            } catch (JSONException exception) {
                throw new IllegalStateException(exception);
            }
        });
    }

    // Leaves the controls when the app takes itself out of them or the activity ends.
    static void release() {
        if (current == null) {
            return;
        }
        current.activity.getSystemService(NotificationManager.class).cancel(NOTIFICATION);
        current.session.release();
        current = null;
    }

    private void show(JSONObject info, byte[] artwork) {
        MediaMetadata.Builder metadata = new MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE, info.optString("title")).putString(MediaMetadata.METADATA_KEY_ARTIST, info.optString("artist")).putString(MediaMetadata.METADATA_KEY_ALBUM, info.optString("album"));
        long duration = Math.round(info.optDouble("duration", 0.0) * 1000.0);
        if (duration > 0) {
            metadata.putLong(MediaMetadata.METADATA_KEY_DURATION, duration);
        }
        Bitmap art = artwork != null && artwork.length > 0 ? BitmapFactory.decodeByteArray(artwork, 0, artwork.length) : null;
        if (art != null) {
            metadata.putBitmap(MediaMetadata.METADATA_KEY_ART, art);
        }
        session.setMetadata(metadata.build());

        int commands = info.optInt("commands");
        long actions = 0;
        for (int index = 0; index < ACTIONS.length; ++index) {
            if ((commands & (1 << index)) != 0) {
                actions |= ACTIONS[index];
            }
        }
        boolean playing = info.optBoolean("playing");
        long position = Math.round(info.optDouble("position", 0.0) * 1000.0);
        session.setPlaybackState(new PlaybackState.Builder().setActions(actions).setState(playing ? PlaybackState.STATE_PLAYING : PlaybackState.STATE_PAUSED, position, playing ? 1.0f : 0.0f).build());
        session.setActive(true);
        notifyShade(info, art);
    }

    // The notification opens the activity of the app when tapped, and stays while the session lives.
    private void notifyShade(JSONObject info, Bitmap art) {
        NotificationManager manager = activity.getSystemService(NotificationManager.class);
        if (manager.getNotificationChannel(CHANNEL) == null) {
            NotificationChannel channel = new NotificationChannel(CHANNEL, activity.getString(R.string.haylen_media_channel), NotificationManager.IMPORTANCE_LOW);
            channel.setShowBadge(false);
            manager.createNotificationChannel(channel);
        }
        Intent open = new Intent(activity, activity.getClass()).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent content = PendingIntent.getActivity(activity, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder builder = new Notification.Builder(activity, CHANNEL).setSmallIcon(activity.getApplicationInfo().icon).setContentTitle(info.optString("title")).setContentText(info.optString("artist")).setContentIntent(content).setVisibility(Notification.VISIBILITY_PUBLIC).setOngoing(info.optBoolean("playing")).setStyle(new Notification.MediaStyle().setMediaSession(session.getSessionToken()));
        if (art != null) {
            builder.setLargeIcon(art);
        }
        manager.notify(NOTIFICATION, builder.build());
    }

    @Override
    public void onPlay() {
        nativeCommand(PLAY, 0.0);
    }

    @Override
    public void onPause() {
        nativeCommand(PAUSE, 0.0);
    }

    @Override
    public void onStop() {
        nativeCommand(STOP, 0.0);
    }

    @Override
    public void onSkipToNext() {
        nativeCommand(NEXT, 0.0);
    }

    @Override
    public void onSkipToPrevious() {
        nativeCommand(PREVIOUS, 0.0);
    }

    @Override
    public void onSeekTo(long position) {
        nativeCommand(SEEK, position / 1000.0);
    }

    // A headset button that toggles arrives as one key, which Android hands to `onPlay` or `onPause` only when the app offers them, so it goes to the toggle of the app here.
    @Override
    public boolean onMediaButtonEvent(Intent intent) {
        KeyEvent key = Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU ? intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT, KeyEvent.class) : legacyKey(intent);
        if (key != null && key.getAction() == KeyEvent.ACTION_DOWN && (key.getKeyCode() == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE || key.getKeyCode() == KeyEvent.KEYCODE_HEADSETHOOK)) {
            nativeCommand(TOGGLE, 0.0);
            return true;
        }
        return super.onMediaButtonEvent(intent);
    }

    @SuppressWarnings("deprecation")
    private static KeyEvent legacyKey(Intent intent) {
        return intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
    }

    private static native void nativeCommand(int command, double position);
}
