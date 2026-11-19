package dev.haylen;

import android.content.pm.ServiceInfo;
import android.os.Build;

// The audio of an app in the background, which the app declares to Android with a service of the type `mediaPlayback` in its merged manifest, such as the service that a plugin of media controls brings to keep the app in the foreground while it plays. The engine keeps its output running in the background exactly when the app declares it, and an app that plays there keeps the audio focus while it is away.
final class HaylenBackgroundAudio {
    private static final String MEDIA_PLAYBACK_PERMISSION = "android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK";

    // Whether the app plays in the background now, which the frame thread sets when an app starts and whenever the app turns it on or off within its declaration.
    private static volatile boolean enabled;

    private HaylenBackgroundAudio() {}

    // Called from the frame thread when an app starts. Android 8.1 and 9 record no type of a service, so there the declaration is the permission of the type, which every manifest that declares such a service for Android 14 holds.
    static boolean isDeclared() {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            return false;
        }

        HaylenRequirements requirements = new HaylenRequirements(activity, "The audio that plays in the background");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return requirements.hasServiceOfType(ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        }

        return requirements.hasPermission(MEDIA_PLAYBACK_PERMISSION);
    }

    // Called from the frame thread when an app starts and whenever it turns the audio in the background on or off.
    static void setEnabled(boolean value) {
        enabled = value;
    }

    static boolean isEnabled() {
        return enabled;
    }
}
