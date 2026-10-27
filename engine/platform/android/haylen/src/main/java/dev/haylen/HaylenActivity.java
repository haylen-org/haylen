package dev.haylen;

import android.Manifest;
import android.app.ActivityManager;
import android.app.UiModeManager;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.CombinedVibration;
import android.os.LocaleList;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import androidx.activity.OnBackPressedCallback;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import com.google.androidgamesdk.GameActivity;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.TimeZone;
import org.json.JSONArray;
import org.json.JSONObject;

// Hosts a Haylen app. The class `GameActivity`, an `AppCompatActivity`, loads the native library that the `android.app.lib_name` meta-data names and draws the app in a `SurfaceView` of an ordinary view hierarchy, so plugins place views over the app, register Activity Result launchers and use fragments, dialogs and Compose.
// The manifest gives the activity the `Theme.Haylen.Splash` theme, whose splash screen hands over to a view with the same look that stays until the app has drawn its first frame.
// The template declares the activity single top, and `HaylenLinkActivity` hands it the links and notifications that open the app, so a screen of a plugin that shows over the app outlives the launcher icon and the links.
public class HaylenActivity extends GameActivity implements InputManager.InputDeviceListener {
    // The extra of the launch intent that names the development server of a debuggable app.
    private static final String DEVELOPMENT_SERVER_EXTRA = "dev.haylen.developmentServer";

    // The names of the values of the status bar, in the order of `Window::StatusBar` in the engine.
    private static final String[] STATUS_BARS = {"hidden", "auto", "light", "dark"};
    private static final int STATUS_BAR_HIDDEN = 0;
    private static final int STATUS_BAR_AUTO = 1;
    private static final int STATUS_BAR_DARK = 3;

    // The link that `HaylenLinkActivity` received while no activity ran, which the next activity of the process takes. A launch that brings an existing task to the front keeps the intent that the task started with, even when Android restores its activity after the end of the process, so the link waits here instead of in the intent. Only the main thread reaches it.
    private static Intent pendingLink;

    private final HaylenSplash splash = new HaylenSplash(this);
    private final HaylenBattery battery = new HaylenBattery(this);
    private final HaylenAccessibility accessibility = new HaylenAccessibility(this);
    private HaylenAccessibilityTree accessibilityTree;
    private HaylenRequirements requirements;
    private HaylenOverlayLayer overlays;
    private HaylenEditText editor;
    private HaylenNetwork network;
    private HaylenFold fold;
    private OnBackPressedCallback backCallback;
    private int statusBar;
    // Whether the app keeps the screen on, which outlives the activity, so an activity that the system creates again keeps the screen on too.
    private static volatile boolean screenKeptOn;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        splash.install();
        requirements = new HaylenRequirements(this, "The engine");
        HaylenBridge.attach(this);
        // The class `GameActivity` loads the native library here, whose runtime starts the frame thread of the app.
        super.onCreate(savedInstanceState);
        // A new activity starts with the link as its intent, while a restored one hands it to the plugins as a new intent once they heard of the activity.
        Intent link = pendingLink;
        pendingLink = null;
        if (link != null && savedInstanceState == null) {
            setIntent(link);
        }
        nativeTelevision(isTelevision(this));
        nativeDensity(getResources().getDisplayMetrics().density);
        nativeOrientation(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT);
        nativeTheme(isDark(getResources().getConfiguration()));
        battery.register();
        accessibility.register();
        accessibilityTree.reportWanted();
        fold = new HaylenFold(this);
        fold.register();

        // The views over the app share the layout of its surface, above it.
        FrameLayout content = findViewById(contentViewId);
        overlays = new HaylenOverlayLayer(this, content);
        editor = new HaylenEditText(this);
        content.addView(editor, new FrameLayout.LayoutParams(1, 1));
        HaylenEditText.attach(editor);
        splash.show();
        ViewCompat.setOnApplyWindowInsetsListener(getWindow().getDecorView(), this::applyInsets);

        backCallback = new OnBackPressedCallback(false) {
            @Override
            public void handleOnBackPressed() {
                onBack();
            }
        };
        getOnBackPressedDispatcher().addCallback(this, backCallback);
        getSystemService(InputManager.class).registerInputDeviceListener(this, null);
        for (int deviceId : InputDevice.getDeviceIds()) {
            onInputDeviceAdded(deviceId);
        }
        followNetwork();
        HaylenPlugins.checkLoaded(getApplication());

        // The audio focus follows the lifecycle before the plugins do, so it is requested before they hear of a resume and abandoned after they hear of a pause. The launchers of the dialogs and of the screens of plugins are registered before the activity starts, like the launchers of the plugins.
        getLifecycle().addObserver(new HaylenAudioFocus(this));
        HaylenDialogs.activityCreated(this);
        HaylenScreens.activityCreated(this);
        HaylenPlugins.activityCreated(this, savedInstanceState);
        if (link != null && savedInstanceState != null) {
            receiveIntent(link);
        }
    }

    // The class `GameActivity` sets up the window before it creates the activity. The app draws edge to edge, under the cutout, with the system bars hidden until a swipe shows them, besides the status bar that the app shows, and the software keyboard lies over the app without resizing it, while the engine lifts the focused field above it.
    @Override
    protected void onSetUpWindow() {
        Window window = getWindow();
        window.setFormat(PixelFormat.RGBX_8888);
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
        WindowCompat.setDecorFitsSystemWindows(window, false);
        WindowManager.LayoutParams attributes = window.getAttributes();
        coverCutouts(attributes);
        window.setAttributes(attributes);
        WindowInsetsControllerCompat controller = WindowCompat.getInsetsController(window, window.getDecorView());
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        statusBar = launchStatusBar();
        applyStatusBar();
        applyScreenKeptOn();
    }

    private void applyScreenKeptOn() {
        if (screenKeptOn) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }

    // The meta-data `dev.haylen.statusBar` of the activity, which the template fills from `app.json`, launches the app with its status bar until the engine applies the same value, and without it the status bar starts hidden.
    private int launchStatusBar() {
        try {
            Bundle metaData = getPackageManager().getActivityInfo(getComponentName(), PackageManager.GET_META_DATA).metaData;
            String name = metaData == null ? null : metaData.getString("dev.haylen.statusBar");
            return Math.max(0, Arrays.asList(STATUS_BARS).indexOf(name));
        } catch (PackageManager.NameNotFoundException exception) {
            throw new IllegalStateException(exception);
        }
    }

    // A hidden status bar shows over the app for a while after a swipe, like the navigation bar, and a status bar that shows has light or dark content, which follows the night mode of the system for `auto`.
    private void applyStatusBar() {
        Window window = getWindow();
        WindowInsetsControllerCompat controller = WindowCompat.getInsetsController(window, window.getDecorView());
        if (statusBar == STATUS_BAR_HIDDEN) {
            controller.hide(WindowInsetsCompat.Type.statusBars());
            return;
        }
        controller.show(WindowInsetsCompat.Type.statusBars());
        controller.setAppearanceLightStatusBars(statusBar == STATUS_BAR_DARK || (statusBar == STATUS_BAR_AUTO && !isDark(getResources().getConfiguration())));
    }

    @Override
    protected InputEnabledSurfaceView createSurfaceView() {
        AppSurface surface = new AppSurface();
        accessibilityTree = HaylenAccessibilityTree.attach(surface);
        return surface;
    }

    // A launch while the activity is on top, such as the launcher icon, arrives here and becomes its intent.
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        receiveIntent(intent);
    }

    // The launchers of the Activity Result API start their activities through these methods, which hand the request code to the screen of a plugin that opens, so a screen that the app gives up finishes its activity. They stay deprecated for callers, who use the Activity Result API.
    @Deprecated
    @Override
    public void startActivityForResult(Intent intent, int requestCode, Bundle options) {
        HaylenScreens.activityStarted(requestCode);
        super.startActivityForResult(intent, requestCode, options);
    }

    @Deprecated
    @Override
    public void startIntentSenderForResult(IntentSender intent, int requestCode, Intent fillInIntent, int flagsMask, int flagsValues, int extraFlags, Bundle options) throws IntentSender.SendIntentException {
        HaylenScreens.activityStarted(requestCode);
        super.startIntentSenderForResult(intent, requestCode, fillInIntent, flagsMask, flagsValues, extraFlags, options);
    }

    // The manifest of the template lets the activity handle a change of the UI mode itself, which is how the night mode of the system reaches it.
    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        nativeDensity(configuration.densityDpi / (float) DisplayMetrics.DENSITY_DEFAULT);
        nativeOrientation(configuration.orientation == Configuration.ORIENTATION_PORTRAIT);
        nativeTheme(isDark(configuration));
        applyStatusBar();
        accessibility.report();
        overlays.refresh();
        HaylenPlugins.configurationChanged(configuration);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        HaylenPlugins.windowFocusChanged(hasFocus);
    }

    // A release goes to the app before the views, which takes it when it took the press, so a key never stays held in the app when a view took the focus between the two, such as the hidden field of a text field that the press started editing. The views get every other release, and back stays with the back callback.
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP && event.getKeyCode() != KeyEvent.KEYCODE_BACK && onKeyUp(event.getKeyCode(), event)) {
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    // Controllers send their sticks, triggers and hats to the focused view, which may be a view over the app, so they reach the app wherever the focus is.
    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK)) {
            return processMotionEvent(event);
        }
        return super.dispatchGenericMotionEvent(event);
    }

    // The app stops inside `GameActivity`, which runs its cleanup and lets the activity finish, while the bridge, the editor and the listeners it may still use are attached. The plugins hear of it next, and the views they left over the app and the messages that still show go with the activity.
    @Override
    protected void onDestroy() {
        super.onDestroy();
        HaylenPlugins.activityDestroyed(this);
        overlays.removeAll();
        HaylenDialogs.activityDestroyed();
        splash.dismiss();
        battery.unregister();
        accessibility.unregister();
        accessibilityTree.detach();
        HaylenMediaSession.release();
        fold.unregister();
        getSystemService(InputManager.class).unregisterInputDeviceListener(this);
        if (network != null) {
            network.unregister();
        }
        HaylenEditText.detach(editor);
        HaylenBridge.detach(this);
    }

    // Hiding the interface is the only trim level that says nothing about memory pressure.
    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (level != TRIM_MEMORY_UI_HIDDEN) {
            nativeLowMemory();
        }
        HaylenPlugins.trimMemory(level);
    }

    @Override
    // A controller connects as soon as Android adds it, with its name, while remotes and keyboards stay keys.
    public void onInputDeviceAdded(int deviceId) {
        InputDevice device = InputDevice.getDevice(deviceId);
        if (device == null) {
            return;
        }
        int sources = device.getSources();
        if ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
            nativeControllerAdded(deviceId, device.getName().getBytes(StandardCharsets.UTF_8), device.getVendorId());
        }
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        nativeControllerRemoved(deviceId);
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {}

    // Called from the frame thread of the engine with 0 for landscape, 1 for portrait and 2 for any orientation, the values `app.json` gives the manifest.
    static void lockOrientation(int value) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            return;
        }
        int requested = value == 0 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE : value == 1 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
        activity.runOnUiThread(() -> activity.setRequestedOrientation(requested));
    }

    // Called from the frame thread of the engine with the value of `Window::StatusBar`, which follows the order of the names in `STATUS_BARS`.
    static void setStatusBar(int value) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> {
            activity.statusBar = value;
            activity.applyStatusBar();
        });
    }

    // Called from the frame thread of the engine when the app keeps the screen on or leaves it to the system.
    static void keepScreenOn(boolean value) {
        screenKeptOn = value;
        HaylenActivity activity = HaylenBridge.activity();
        if (activity != null) {
            activity.runOnUiThread(activity::applyScreenKeptOn);
        }
    }

    // Called from the frame thread of the engine whenever the app starts or stops taking the back button, which it takes while it captures back or edits a text field. The back callback takes back while it is enabled, and otherwise Android leaves the app with its predictive back animation.
    static void captureBack(boolean captured) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity != null) {
            activity.runOnUiThread(() -> activity.backCallback.setEnabled(captured));
        }
    }

    // Called from the frame thread of the engine when the first app of the process starts, with what Android tells about the device as the JSON that `AndroidDeviceInfo` reads. A tablet is a device whose smallest screen side has the 600 dp that the tablet layouts of Android start at, the processor has a name from Android 12 on, and a value that the device reports as `Build.UNKNOWN` stays unknown.
    static byte[] systemInfo() {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            return "{}".getBytes(StandardCharsets.UTF_8);
        }
        ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
        activity.getSystemService(ActivityManager.class).getMemoryInfo(memory);
        LocaleList locales = LocaleList.getDefault();
        JSONArray languages = new JSONArray();
        for (int index = 0; index < locales.size(); ++index) {
            languages.put(locales.get(index).toLanguageTag());
        }

        Map<String, Object> info = new HashMap<>();
        info.put("osVersion", Build.VERSION.RELEASE);
        info.put("deviceModel", Build.MODEL);
        if (!Build.MANUFACTURER.equals(Build.UNKNOWN)) {
            info.put("manufacturer", Build.MANUFACTURER);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && !Build.SOC_MODEL.equals(Build.UNKNOWN)) {
            info.put("cpuName", Build.SOC_MODEL);
        }
        info.put("cpuCores", Runtime.getRuntime().availableProcessors());
        info.put("memoryBytes", memory.totalMem);
        info.put("television", isTelevision(activity));
        info.put("tablet", activity.getResources().getConfiguration().smallestScreenWidthDp >= 600);
        info.put("locale", Locale.getDefault().toLanguageTag());
        info.put("languages", languages);
        info.put("timeZone", TimeZone.getDefault().getID());
        return new JSONObject(info).toString().getBytes(StandardCharsets.UTF_8);
    }

    // Called from the frame thread of the engine, which hears through `nativeUrlOpened` with the same request whether an app took the url.
    static void openUrl(long request, byte[] url) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            nativeUrlOpened(request, false);
            return;
        }
        String address = new String(url, StandardCharsets.UTF_8);
        activity.runOnUiThread(() -> {
            try {
                activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(address)));
                nativeUrlOpened(request, true);
            } catch (RuntimeException error) {
                // No app takes the url, or Android refuses it, such as a file url that would leave the app.
                nativeUrlOpened(request, false);
            }
        });
    }

    // Called from the frame thread of the engine, which puts text on the clipboard of the system.
    static void setClipboard(byte[] text) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity != null) {
            activity.getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("text", new String(text, StandardCharsets.UTF_8)));
        }
    }

    // Called from the frame thread of the engine, which reads the text on the clipboard of the system, empty when it holds none or Android keeps it from an app without the focus.
    static byte[] clipboard() {
        HaylenActivity activity = HaylenBridge.activity();
        ClipData clip = activity != null ? activity.getSystemService(ClipboardManager.class).getPrimaryClip() : null;
        CharSequence text = clip != null && clip.getItemCount() > 0 ? clip.getItemAt(0).coerceToText(activity) : "";
        return text.toString().getBytes(StandardCharsets.UTF_8);
    }

    // Called from the frame thread of the engine. Devices without a vibrator do nothing, and so does an app without the permission `VIBRATE`, which the log tells once.
    static void vibrate(long milliseconds) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            return;
        }
        if (!activity.requirements.isGranted(Manifest.permission.VIBRATE)) {
            activity.requirements.report(HaylenRequirements.Requirement.permission(Manifest.permission.VIBRATE), Log.WARN, "\"system.vibrate\" does nothing");
            return;
        }
        Vibrator vibrator = activity.getSystemService(Vibrator.class);
        if (vibrator != null && vibrator.hasVibrator()) {
            vibrator.vibrate(VibrationEffect.createOneShot(milliseconds, VibrationEffect.DEFAULT_AMPLITUDE));
        }
    }

    // Called from the frame thread of the engine with the low and the high frequency motors of a controller from 0 to 1, where both at 0 stop it. A controller with two vibrators gets one motor on each, one with a single vibrator gets the stronger motor, and a controller without one, or an app without the permission `VIBRATE`, which the log tells once, does nothing.
    static void rumbleGamepad(int deviceId, float low, float high, long milliseconds) {
        HaylenActivity activity = HaylenBridge.activity();
        InputDevice device = InputDevice.getDevice(deviceId);
        if (activity == null || device == null) {
            return;
        }
        if (!activity.requirements.isGranted(Manifest.permission.VIBRATE)) {
            activity.requirements.report(HaylenRequirements.Requirement.permission(Manifest.permission.VIBRATE), Log.WARN, "\"input.rumble\" does nothing");
            return;
        }
        boolean stop = low <= 0.0f && high <= 0.0f;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager manager = device.getVibratorManager();
            int[] ids = manager.getVibratorIds();
            if (ids.length == 0) {
                return;
            }
            if (stop) {
                manager.cancel();
                return;
            }
            CombinedVibration.ParallelCombination combination = CombinedVibration.startParallel();
            float first = ids.length > 1 ? low : Math.max(low, high);
            if (first > 0.0f) {
                combination.addVibrator(ids[0], oneShot(milliseconds, first));
            }
            if (ids.length > 1 && high > 0.0f) {
                combination.addVibrator(ids[1], oneShot(milliseconds, high));
            }
            manager.vibrate(combination.combine());
            return;
        }
        rumbleBeforeVibratorManagers(device, stop, Math.max(low, high), milliseconds);
    }

    // Android before 12 has no vibrator manager on input devices and reaches the motors of a controller only as one vibrator.
    @SuppressWarnings("deprecation")
    private static void rumbleBeforeVibratorManagers(InputDevice device, boolean stop, float intensity, long milliseconds) {
        Vibrator vibrator = device.getVibrator();
        if (!vibrator.hasVibrator()) {
            return;
        }
        if (stop) {
            vibrator.cancel();
            return;
        }
        vibrator.vibrate(oneShot(milliseconds, intensity));
    }

    // Android takes amplitudes from 1 to 255, so a weak motor still turns at the lowest one.
    private static VibrationEffect oneShot(long milliseconds, float intensity) {
        return VibrationEffect.createOneShot(Math.max(1, milliseconds), Math.max(1, Math.round(intensity * 255.0f)));
    }

    // Called from the frame thread of the engine when an app starts, with the sentence that the network errors of the engine end with while the app lacks the permission `INTERNET`, or nothing.
    static byte[] networkRequirement() {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null || activity.requirements.isGranted(Manifest.permission.INTERNET)) {
            return new byte[0];
        }
        HaylenRequirements.Requirement internet = HaylenRequirements.Requirement.permission(Manifest.permission.INTERNET);
        return ("The app lacks " + internet.description() + ", which network access needs on Android. " + internet.instructions()).getBytes(StandardCharsets.UTF_8);
    }

    // Called from the frame thread of the engine when an app starts, with the WebSocket address of the development server that `haylen.py run` launches the app with, or nothing. Only a debuggable app reads it, so an app that ships never runs in development.
    static byte[] developmentServer() {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null || (activity.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0) {
            return new byte[0];
        }
        String address = activity.getIntent().getStringExtra(DEVELOPMENT_SERVER_EXTRA);
        return address == null ? new byte[0] : address.getBytes(StandardCharsets.UTF_8);
    }

    // Called by `HaylenLinkActivity` on the main thread while no activity runs.
    static void keepLink(Intent link) {
        pendingLink = link;
    }

    // A link or a notification that reaches the running app, which becomes the intent of the activity before the plugins hear of it.
    void receiveIntent(Intent intent) {
        setIntent(intent);
        HaylenPlugins.newIntent(intent);
    }

    HaylenOverlayLayer overlays() {
        return overlays;
    }

    // Following the network needs the permission `ACCESS_NETWORK_STATE`, without which the engine never hears of the network, which the log tells once.
    private void followNetwork() {
        if (!requirements.isGranted(Manifest.permission.ACCESS_NETWORK_STATE)) {
            requirements.report(HaylenRequirements.Requirement.permission(Manifest.permission.ACCESS_NETWORK_STATE), Log.INFO, "the network state stays \"unknown\" and the events \"networkOnline\" and \"networkOffline\" never fire");
            return;
        }
        network = new HaylenNetwork(this);
        network.register();
    }

    // Hidden bars only appear transiently over the app, so the safe area covers the cutout and any bars that stay visible, as in multi-window mode.
    private WindowInsetsCompat applyInsets(View view, WindowInsetsCompat insets) {
        Insets safe = insets.getInsets(WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout());
        nativeSafeArea(safe.left, safe.top, safe.right, safe.bottom, insets.getInsets(WindowInsetsCompat.Type.statusBars()).top);
        overlays.setSafeArea(safe);

        boolean keyboardShown = insets.isVisible(WindowInsetsCompat.Type.ime());
        int keyboardHeight = keyboardShown ? insets.getInsets(WindowInsetsCompat.Type.ime()).bottom : 0;
        nativeKeyboard(0, view.getHeight() - keyboardHeight, keyboardHeight > 0 ? view.getWidth() : 0, keyboardHeight);
        editor.setKeyboardShown(keyboardShown);
        return ViewCompat.onApplyWindowInsets(view, insets);
    }

    // Back lets the text field being edited go first, and otherwise reaches the engine as escape.
    private void onBack() {
        if (!editor.dismiss()) {
            nativeBack();
        }
    }

    // A TV runs in the television UI mode, and some TV devices tell it only through the leanback feature.
    private static boolean isTelevision(Context context) {
        return context.getSystemService(UiModeManager.class).getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION || context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK);
    }

    private static boolean isDark(Configuration configuration) {
        return (configuration.uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }

    // Android 11 lets a window cover every cutout, Android 9 and 10 only those on the short edges, and earlier versions have no cutouts.
    private static void coverCutouts(WindowManager.LayoutParams attributes) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    // The surface the app draws on. The class `GameActivity` focuses it whenever its window gains the focus, which would take the focus and the keyboard from a text field over the app, such as the hidden field that edits a field of the UI or a form of a plugin, so it declines the focus while such a field has it.
    // Android marks the focused view with a highlight over it once keys drive the window, as on a TV, which would veil the whole app, while the app shows its own focus.
    private final class AppSurface extends InputEnabledSurfaceView {
        AppSurface() {
            super(HaylenActivity.this);
            setDefaultFocusHighlightEnabled(false);
        }

        // Screen readers that explore by touch hover over the elements of the UI.
        @Override
        public boolean dispatchHoverEvent(MotionEvent event) {
            return accessibilityTree.dispatchHoverEvent(event) || super.dispatchHoverEvent(event);
        }

        @Override
        public boolean requestFocus(int direction, Rect previouslyFocusedRect) {
            View focused = getCurrentFocus();
            if (focused != null && focused != this && focused.onCheckIsTextEditor()) {
                return false;
            }
            return super.requestFocus(direction, previouslyFocusedRect);
        }
    }

    private static native void nativeSafeArea(int left, int top, int right, int bottom, int statusBar);

    private static native void nativeControllerAdded(int deviceId, byte[] name, int vendorId);

    private static native void nativeControllerRemoved(int deviceId);

    private static native void nativeTelevision(boolean television);

    private static native void nativeLowMemory();

    private static native void nativeKeyboard(int x, int y, int width, int height);

    private static native void nativeDensity(float density);

    private static native void nativeOrientation(boolean portrait);

    private static native void nativeBack();

    private static native void nativeUrlOpened(long request, boolean opened);

    private static native void nativeTheme(boolean dark);
}
