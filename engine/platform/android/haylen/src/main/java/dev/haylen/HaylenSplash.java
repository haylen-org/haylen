package dev.haylen;

import android.app.Activity;
import android.view.Choreographer;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import androidx.core.splashscreen.SplashScreen;

// Keeps the splash screen of `Theme.Haylen.Splash` on screen until the app has drawn its first frame and the splash of `app.json` lasted its duration, so nothing black shows in between, and fades it out into the app.
// The system splash screen ends with the first frame of the activity window, while the app draws only once the activity resumed and its surface exists, so a view with the same background and icon covers the surface of the app from the first frame of the window until the engine ends the splash.
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

    // The cover fades out over the seconds of `app.json`, and a cover that the activity takes away meanwhile goes at once.
    private void removeOnceAppDrew(long frameTimeNanos) {
        if (cover == null) {
            return;
        }
        if (!nativeFramePresented()) {
            Choreographer.getInstance().postFrameCallback(this::removeOnceAppDrew);
            return;
        }
        cover.animate().alpha(0).setDuration(Math.round(nativeFadeOut() * 1000.0)).withEndAction(this::dismiss);
    }

    private static native boolean nativeFramePresented();

    private static native float nativeFadeOut();
}
