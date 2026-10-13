package dev.haylen;

import android.app.Activity;
import android.app.UiModeManager;
import android.database.ContentObserver;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.accessibility.AccessibilityManager;
import androidx.core.content.ContextCompat;

// Tells the engine the accessibility settings of the system while the activity lives: the font scale of the configuration, the animator duration scale of the global settings, whose 0 means the user removed animations, the contrast of the UI mode on Android 14 and newer, and whether touch exploration, which screen readers turn on, runs. Android has no setting for reduced transparency.
final class HaylenAccessibility implements AccessibilityManager.TouchExplorationStateChangeListener {
    private final Activity activity;
    private final ContentObserver animations = new ContentObserver(new Handler(Looper.getMainLooper())) {
        @Override
        public void onChange(boolean selfChange) {
            report();
        }
    };
    private AccessibilityManager manager;
    private UiModeManager.ContrastChangeListener contrast;

    HaylenAccessibility(Activity activity) {
        this.activity = activity;
    }

    // The system services of the activity exist once it is created, which is when it registers.
    void register() {
        manager = activity.getSystemService(AccessibilityManager.class);
        manager.addTouchExplorationStateChangeListener(this);
        activity.getContentResolver().registerContentObserver(Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE), false, animations);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            contrast = level -> report();
            activity.getSystemService(UiModeManager.class).addContrastChangeListener(ContextCompat.getMainExecutor(activity), contrast);
        }
        report();
    }

    void unregister() {
        manager.removeTouchExplorationStateChangeListener(this);
        activity.getContentResolver().unregisterContentObserver(animations);
        if (contrast != null) {
            activity.getSystemService(UiModeManager.class).removeContrastChangeListener(contrast);
        }
    }

    @Override
    public void onTouchExplorationStateChanged(boolean enabled) {
        report();
    }

    // The activity handles changes of the font scale itself, so it reports them from its configuration changes.
    void report() {
        float textScale = activity.getResources().getConfiguration().fontScale;
        boolean reducedMotion = Settings.Global.getFloat(activity.getContentResolver(), Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f) == 0.0f;
        boolean highContrast = Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE && activity.getSystemService(UiModeManager.class).getContrast() > 0.0f;
        nativeAccessibility(textScale, reducedMotion, highContrast, manager.isTouchExplorationEnabled());
    }

    private static native void nativeAccessibility(float textScale, boolean reducedMotion, boolean highContrast, boolean screenReader);
}
