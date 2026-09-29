package dev.haylen;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;

// Holds the audio focus while the app is in the foreground. When another app takes it, such as a phone call or an alarm, the app is interrupted: the engine makes it inactive until the focus comes back. A loss that lets the app play quieter needs nothing, since the system lowers the volume itself.
final class HaylenAudioFocus implements AudioManager.OnAudioFocusChangeListener {
    private final AudioManager manager;
    private final AudioFocusRequest request;
    private boolean interrupted;

    HaylenAudioFocus(Context context) {
        manager = context.getSystemService(AudioManager.class);
        AudioAttributes attributes = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build();
        request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attributes).setOnAudioFocusChangeListener(this).build();
    }

    // Called as the app comes to the foreground. A focus that is granted right away also ends an interruption that happened while the app was away.
    void request() {
        if (manager.requestAudioFocus(request) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            setInterrupted(false);
        }
    }

    void abandon() {
        manager.abandonAudioFocusRequest(request);
    }

    @Override
    public void onAudioFocusChange(int change) {
        if (change == AudioManager.AUDIOFOCUS_GAIN) {
            setInterrupted(false);
        } else if (change == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT || change == AudioManager.AUDIOFOCUS_LOSS) {
            setInterrupted(true);
        }
    }

    private void setInterrupted(boolean value) {
        if (interrupted != value) {
            interrupted = value;
            nativeInterruption(value);
        }
    }

    private static native void nativeInterruption(boolean began);
}
