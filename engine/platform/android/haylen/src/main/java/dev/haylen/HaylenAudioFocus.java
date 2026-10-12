package dev.haylen;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import androidx.annotation.NonNull;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

// Holds the audio focus while the activity is resumed, as an observer of its lifecycle. When another app takes it, such as a phone call or an alarm, the app is interrupted: the engine makes it inactive until the focus comes back. A request that the system delays or refuses, such as during a call, interrupts the app as well, and a delayed one ends the interruption once the system grants it. A lasting loss, such as music that the player starts in another app, keeps the app interrupted until it comes back to the foreground. A loss that lets the app play quieter needs nothing, since the system lowers the volume itself.
final class HaylenAudioFocus implements AudioManager.OnAudioFocusChangeListener, DefaultLifecycleObserver {
    private final AudioManager manager;
    private final AudioFocusRequest request;
    private boolean interrupted;

    HaylenAudioFocus(Context context) {
        manager = context.getSystemService(AudioManager.class);
        AudioAttributes attributes = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build();
        request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attributes).setAcceptsDelayedFocusGain(true).setOnAudioFocusChangeListener(this).build();
    }

    // A focus that is granted right away also ends an interruption that happened while the app was away.
    @Override
    public void onResume(@NonNull LifecycleOwner owner) {
        setInterrupted(manager.requestAudioFocus(request) != AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
    }

    @Override
    public void onPause(@NonNull LifecycleOwner owner) {
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
