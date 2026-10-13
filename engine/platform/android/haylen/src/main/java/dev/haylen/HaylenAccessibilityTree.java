package dev.haylen;

import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeProviderCompat;
import androidx.customview.widget.ExploreByTouchHelper;
import java.nio.charset.StandardCharsets;
import java.util.List;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// Serves the elements of the UI of the app to TalkBack and the other accessibility services as virtual views of the surface of the app. The engine hands the tree whenever it changes, and the actions that services ask of an element reach the engine, which performs them the way the keyboard and gamepads do. The engine builds the tree while an accessibility service runs or once a service asked for the views.
final class HaylenAccessibilityTree extends ExploreByTouchHelper implements AccessibilityManager.AccessibilityStateChangeListener {
    // The bits of the states and actions of the engine.
    private static final int CHECKED = 1 << 1;
    private static final int SELECTED = 1 << 2;
    private static final int DISABLED = 1;
    private static final int FOCUSED = 1 << 3;
    private static final int EXPANDED = 1 << 4;
    private static final int ACTION_FOCUS = 1;
    private static final int ACTION_ACTIVATE = 1 << 1;
    private static final int ACTION_INCREMENT = 1 << 2;
    private static final int ACTION_DECREMENT = 1 << 3;

    private static final Handler main = new Handler(Looper.getMainLooper());
    private static volatile HaylenAccessibilityTree current;

    private record Element(long id, String role, String label, String value, String hint, Rect bounds, int states, int actions) {}

    private final View view;
    private final AccessibilityManager manager;
    private Element[] elements = new Element[0];
    private long focused;
    private boolean asked;

    private HaylenAccessibilityTree(View view) {
        super(view);
        this.view = view;
        manager = view.getContext().getSystemService(AccessibilityManager.class);
    }

    // Serves the elements on the surface of the activity from now on, until it detaches. The activity creates its surface before it loads the native library, so it reports whether it wants the tree once the library is loaded.
    static HaylenAccessibilityTree attach(View view) {
        HaylenAccessibilityTree tree = new HaylenAccessibilityTree(view);
        ViewCompat.setAccessibilityDelegate(view, tree);
        tree.manager.addAccessibilityStateChangeListener(tree);
        current = tree;
        return tree;
    }

    void detach() {
        manager.removeAccessibilityStateChangeListener(this);
        if (current == this) {
            current = null;
            nativeWanted(false);
        }
    }

    @Override
    public void onAccessibilityStateChanged(boolean enabled) {
        reportWanted();
    }

    void reportWanted() {
        nativeWanted(asked || manager.isEnabled());
    }

    // Called by the engine on its frame thread with the JSON of the tree.
    static void update(byte[] json) {
        Element[] parsed;
        long focusedId;
        try {
            JSONObject tree = new JSONObject(new String(json, StandardCharsets.UTF_8));
            JSONArray list = tree.getJSONArray("elements");
            parsed = new Element[list.length()];
            for (int index = 0; index < parsed.length; ++index) {
                JSONObject element = list.getJSONObject(index);
                JSONArray bounds = element.getJSONArray("bounds");
                int left = (int) Math.floor(bounds.getDouble(0));
                int top = (int) Math.floor(bounds.getDouble(1));
                Rect rect = new Rect(left, top, (int) Math.ceil(bounds.getDouble(0) + bounds.getDouble(2)), (int) Math.ceil(bounds.getDouble(1) + bounds.getDouble(3)));
                parsed[index] = new Element(element.getLong("id"), element.getString("role"), element.getString("label"), element.getString("value"), element.getString("hint"), rect, element.getInt("states"), element.getInt("actions"));
            }
            focusedId = tree.getLong("focused");
        } catch (JSONException error) {
            throw new IllegalStateException("The engine handed a tree of accessible elements that is not valid JSON.", error);
        }
        main.post(() -> {
            HaylenAccessibilityTree tree = current;
            if (tree != null) {
                tree.show(parsed, focusedId);
            }
        });
    }

    // Called by the engine on its frame thread.
    static void announce(byte[] text, boolean interrupt) {
        String announcement = new String(text, StandardCharsets.UTF_8);
        main.post(() -> {
            HaylenAccessibilityTree tree = current;
            if (tree != null) {
                tree.view.announceForAccessibility(announcement);
            }
        });
    }

    // Only the elements that changed tell the services so while the elements stay the same ones, and a focus that moved in the UI moves the accessibility focus of a running screen reader along.
    private void show(Element[] parsed, long focusedId) {
        Element[] shown = elements;
        elements = parsed;
        boolean same = shown.length == parsed.length;
        for (int index = 0; same && index < parsed.length; ++index) {
            same = shown[index].id == parsed[index].id;
        }
        if (!same) {
            invalidateRoot();
        } else {
            for (int index = 0; index < parsed.length; ++index) {
                if (!shown[index].equals(parsed[index])) {
                    invalidateVirtualView(index);
                }
            }
        }
        if (focusedId == focused) {
            return;
        }
        focused = focusedId;
        int index = indexOf(focusedId);
        if (index >= 0 && manager.isTouchExplorationEnabled()) {
            AccessibilityNodeProviderCompat provider = getAccessibilityNodeProvider(view);
            if (provider != null) {
                provider.performAction(index, AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null);
            }
        }
    }

    private int indexOf(long id) {
        for (int index = 0; index < elements.length; ++index) {
            if (elements[index].id == id) {
                return index;
            }
        }
        return -1;
    }

    private void markAsked() {
        if (!asked) {
            asked = true;
            reportWanted();
        }
    }

    // The innermost element under the point that does something or says something, which the later elements are.
    @Override
    protected int getVirtualViewAt(float x, float y) {
        markAsked();
        for (int index = elements.length - 1; index >= 0; --index) {
            Element element = elements[index];
            if (element.bounds.contains((int) x, (int) y) && (element.actions != 0 || !element.label.isEmpty())) {
                return index;
            }
        }
        return INVALID_ID;
    }

    @Override
    protected void getVisibleVirtualViews(List<Integer> virtualViewIds) {
        markAsked();
        for (int index = 0; index < elements.length; ++index) {
            Element element = elements[index];
            if (element.actions != 0 || !element.label.isEmpty() || !element.value.isEmpty()) {
                virtualViewIds.add(index);
            }
        }
    }

    // The classes of Android widgets tell services how to describe each role.
    @SuppressWarnings("deprecation")
    @Override
    protected void onPopulateNodeForVirtualView(int virtualViewId, @NonNull AccessibilityNodeInfoCompat node) {
        markAsked();
        Element element = virtualViewId < elements.length ? elements[virtualViewId] : null;
        if (element == null) {
            node.setBoundsInParent(new Rect());
            node.setContentDescription("");
            return;
        }
        node.setClassName(classOf(element.role));
        node.setBoundsInParent(element.bounds);
        if (element.role.equals("textField")) {
            node.setText(element.value);
            node.setHintText(element.label);
        } else {
            node.setContentDescription(element.label);
            if (!element.value.isEmpty()) {
                node.setStateDescription(element.value);
            }
        }
        if (!element.hint.isEmpty()) {
            node.setTooltipText(element.hint);
        }
        node.setHeading(element.role.equals("heading"));
        boolean checkable = element.role.equals("checkBox") || element.role.equals("switch") || element.role.equals("radioButton");
        node.setCheckable(checkable);
        node.setChecked((element.states & CHECKED) != 0);
        node.setSelected((element.states & SELECTED) != 0);
        node.setEnabled((element.states & DISABLED) == 0);
        if ((element.states & EXPANDED) != 0) {
            node.addAction(AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COLLAPSE);
        }
        node.setFocusable((element.actions & ACTION_FOCUS) != 0);
        node.setFocused((element.states & FOCUSED) != 0);
        if ((element.actions & ACTION_ACTIVATE) != 0) {
            node.setClickable(true);
            node.addAction(AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLICK);
        }
        if ((element.actions & ACTION_INCREMENT) != 0) {
            node.addAction(AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_FORWARD);
        }
        if ((element.actions & ACTION_DECREMENT) != 0) {
            node.addAction(AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_BACKWARD);
        }
    }

    @Override
    protected boolean onPerformActionForVirtualView(int virtualViewId, int action, @Nullable Bundle arguments) {
        if (virtualViewId >= elements.length) {
            return false;
        }
        Element element = elements[virtualViewId];
        int engineAction = switch (action) {
            case AccessibilityNodeInfoCompat.ACTION_CLICK -> ACTION_ACTIVATE;
            case AccessibilityNodeInfoCompat.ACTION_SCROLL_FORWARD -> ACTION_INCREMENT;
            case AccessibilityNodeInfoCompat.ACTION_SCROLL_BACKWARD -> ACTION_DECREMENT;
            default -> 0;
        };
        if ((element.actions & engineAction) == 0) {
            return false;
        }
        nativePerform(element.id, engineAction);
        return true;
    }

    // A service that moves its accessibility focus to an element moves the focus of the UI there.
    @Override
    protected void onPopulateEventForVirtualView(int virtualViewId, @NonNull AccessibilityEvent event) {
        if (virtualViewId >= elements.length) {
            return;
        }
        Element element = elements[virtualViewId];
        event.setContentDescription(element.label);
        if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED && (element.actions & ACTION_FOCUS) != 0 && element.id != focused) {
            nativePerform(element.id, ACTION_FOCUS);
        }
    }

    private static String classOf(String role) {
        return switch (role) {
            case "button", "tab" -> "android.widget.Button";
            case "checkBox" -> "android.widget.CheckBox";
            case "switch" -> "android.widget.Switch";
            case "radioButton" -> "android.widget.RadioButton";
            case "radioGroup" -> "android.widget.RadioGroup";
            case "slider" -> "android.widget.SeekBar";
            case "textField" -> "android.widget.EditText";
            case "comboBox" -> "android.widget.Spinner";
            case "progressBar" -> "android.widget.ProgressBar";
            case "image" -> "android.widget.ImageView";
            case "list" -> "android.widget.ListView";
            case "tabList" -> "android.widget.TabWidget";
            case "text", "heading", "alert" -> "android.widget.TextView";
            default -> "android.view.ViewGroup";
        };
    }

    private static native void nativeWanted(boolean wanted);

    private static native void nativePerform(long element, int action);
}
