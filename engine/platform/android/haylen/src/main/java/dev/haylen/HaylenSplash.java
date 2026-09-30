package dev.haylen;

import android.app.Activity;
import android.view.Choreographer;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import androidx.core.splashscreen.SplashScreen;

// Keeps the splash screen of `Theme.Haylen.Splash` on screen until the app has drawn its first frame, so nothing black shows in between.
// The system splash screen ends with the first frame of the activity window, while the app draws only once that window shows and has the focus, so a view with the same background and icon covers the surface of the app from the first frame of the window until the app has drawn.
final class HaylenSplash {
    private final Activity activity;
    private View cover;

    HaylenSplash(Activity activity) {
        this.activity = activity;
    }

    // Called before the activity creates its window, as the SplashScreen API requires.
    void install() {
        SplashScreen.installSplashScreen(activity);
    }

    // Called once the activity has its content, over which the cover shows.
    void show() {
        if (nativeFramePresented()) {
            return;
        }

        // The splash screen draws its icon in a square of 288 dp, and the icon drawable already keeps the logo inside the circle Android masks it with.
        FrameLayout view = new FrameLayout(activity);
        view.setBackgroundColor(activity.getColor(R.color.haylen_splash_background));
        ImageView icon = new ImageView(activity);
        icon.setImageResource(R.drawable.haylen_splash_icon);
        int size = Math.round(288 * activity.getResources().getDisplayMetrics().density);
        view.addView(icon, new FrameLayout.LayoutParams(size, size, Gravity.CENTER));
        activity.addContentView(view, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        cover = view;
        Choreographer.getInstance().postFrameCallback(this::removeOnceAppDrew);
    }

    void dismiss() {
        if (cover != null) {
            ((ViewGroup) cover.getParent()).removeView(cover);
            cover = null;
        }
    }

    private void removeOnceAppDrew(long frameTimeNanos) {
        if (cover == null) {
            return;
        }
        if (nativeFramePresented()) {
            dismiss();
            return;
        }
        Choreographer.getInstance().postFrameCallback(this::removeOnceAppDrew);
    }

    private static native boolean nativeFramePresented();
}
