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
import androidx.core.view.accessibility.AccessibilityEventCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeProviderCompat;
import androidx.customview.widget.ExploreByTouchHelper;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// Serves the elements of the UI of the app to TalkBack and the other accessibility services as virtual views of the surface of the app. The engine hands the tree whenever it changes, and the actions that services ask of an element reach the engine, which performs them the way the keyboard and gamepads do. The engine builds the tree while an accessibility service runs or once a service asked for the views. Each element keeps the id of its virtual view while it stays in the tree, the way a view keeps its id, so a node that a service holds, such as the button that a UI test found, describes and acts on that element alone while elements before it come and go, and the node of an element that left the tree is gone, as the node of a removed view is.
final class HaylenAccessibilityTree extends ExploreByTouchHelper implements AccessibilityManager.AccessibilityStateChangeListener {
    // The bits of the states and actions of the engine.
    private static final int DISABLED = 1;
    private static final int CHECKED = 1 << 1;
    private static final int SELECTED = 1 << 2;
    private static final int FOCUSED = 1 << 3;
    private static final int EXPANDED = 1 << 4;
    private static final int ACTION_FOCUS = 1;
    private static final int ACTION_ACTIVATE = 1 << 1;
    private static final int ACTION_INCREMENT = 1 << 2;
    private static final int ACTION_DECREMENT = 1 << 3;
    // The virtual view that says the announcements of the app as a live region, after the elements.
    private static final int ANNOUNCER = Integer.MAX_VALUE;

    private static final Handler main = new Handler(Looper.getMainLooper());
    private static volatile HaylenAccessibilityTree current;

    private record Element(long id, String role, String label, String value, String hint, Rect bounds, int states, int actions) {}

    private final View view;
    private final AccessibilityManager manager;
    private Element[] elements = new Element[0];
    // The id of the virtual view of each element, in the order of the elements, and the ids by the ids of the elements in the engine.
    private int[] virtualIds = new int[0];
    private Map<Long, Integer> virtualIdsByElement = new HashMap<>();
    private int nextVirtualId;
    private NodeProvider provider;
    private long focused;
    private boolean asked;
    private String announcement = "";
    private boolean interrupting;

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
                double x = bounds.getDouble(0);
                double y = bounds.getDouble(1);
                double width = bounds.getDouble(2);
                double height = bounds.getDouble(3);
                Rect rect = new Rect((int) Math.floor(x), (int) Math.floor(y), (int) Math.ceil(x + width), (int) Math.ceil(y + height));
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

    // Called by the engine on its frame thread. A screen reader says the text of a live region when it changes, politely after what it says or at once for an assertive region.
    static void announce(byte[] text, boolean interrupt) {
        String said = new String(text, StandardCharsets.UTF_8);
        main.post(() -> {
            HaylenAccessibilityTree tree = current;
            if (tree == null) {
                return;
            }

            boolean joins = tree.announcement.isEmpty();
            tree.announcement = said;
            tree.interrupting = interrupt;
            if (joins) {
                tree.invalidateRoot();
            }

            tree.invalidateVirtualView(ANNOUNCER, AccessibilityEventCompat.CONTENT_CHANGE_TYPE_TEXT);
        });
    }

    // An element that the last tree held keeps its virtual view, and a new one takes a new id. Only the elements that changed tell the services so while the tree holds the same elements in the same order, and a focus that moved in the UI moves the accessibility focus of a running screen reader along.
    private void show(Element[] parsed, long focusedId) {
        Element[] shown = elements;
        int[] shownIds = virtualIds;
        int[] ids = new int[parsed.length];
        Map<Long, Integer> idsByElement = new HashMap<>();
        for (int index = 0; index < parsed.length; ++index) {
            long element = parsed[index].id;
            Integer kept = idsByElement.containsKey(element) ? null : virtualIdsByElement.get(element);
            ids[index] = kept != null ? kept : takeVirtualId();
            idsByElement.putIfAbsent(element, ids[index]);
        }

        elements = parsed;
        virtualIds = ids;
        virtualIdsByElement = idsByElement;
        if (!Arrays.equals(shownIds, ids)) {
            invalidateRoot();
        } else {
            for (int index = 0; index < parsed.length; ++index) {
                if (!shown[index].equals(parsed[index])) {
                    invalidateVirtualView(ids[index]);
                }
            }
        }

        if (focusedId == focused) {
            return;
        }

        focused = focusedId;
        int index = indexOf(focusedId);
        if (index >= 0 && manager.isTouchExplorationEnabled()) {
            getAccessibilityNodeProvider(view).performAction(ids[index], AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null);
        }
    }

    // The ids go to new elements in turn below the id of the announcer, and start again from zero after the last one, long after any node of the element that held an id was let go.
    private int takeVirtualId() {
        int id = nextVirtualId;
        nextVirtualId = id == ANNOUNCER - 1 ? 0 : id + 1;
        return id;
    }

    private int indexOf(long id) {
        for (int index = 0; index < elements.length; ++index) {
            if (elements[index].id == id) {
                return index;
            }
        }

        return -1;
    }

    private int indexOfVirtualView(int virtualViewId) {
        for (int index = 0; index < virtualIds.length; ++index) {
            if (virtualIds[index] == virtualViewId) {
                return index;
            }
        }

        return -1;
    }

    private boolean isShown(int virtualViewId) {
        if (virtualViewId == HOST_ID) {
            return true;
        }

        if (virtualViewId == ANNOUNCER) {
            return !announcement.isEmpty();
        }

        return indexOfVirtualView(virtualViewId) >= 0;
    }

    @NonNull
    @Override
    public AccessibilityNodeProviderCompat getAccessibilityNodeProvider(@NonNull View host) {
        if (provider == null) {
            provider = new NodeProvider(super.getAccessibilityNodeProvider(host));
        }

        return provider;
    }

    // The provider of the helper, which answers no node for a virtual view that the tree no longer holds, so a service that asks for a node it kept learns that its element is gone.
    private final class NodeProvider extends AccessibilityNodeProviderCompat {
        private final AccessibilityNodeProviderCompat helper;

        NodeProvider(AccessibilityNodeProviderCompat helper) {
            this.helper = helper;
        }

        @Nullable
        @Override
        public AccessibilityNodeInfoCompat createAccessibilityNodeInfo(int virtualViewId) {
            if (!isShown(virtualViewId)) {
                return null;
            }

            return helper.createAccessibilityNodeInfo(virtualViewId);
        }

        @Override
        public boolean performAction(int virtualViewId, int action, @Nullable Bundle arguments) {
            return helper.performAction(virtualViewId, action, arguments);
        }

        @Nullable
        @Override
        public AccessibilityNodeInfoCompat findFocus(int focus) {
            return helper.findFocus(focus);
        }
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
                return virtualIds[index];
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
                virtualViewIds.add(virtualIds[index]);
            }
        }

        if (!announcement.isEmpty()) {
            virtualViewIds.add(ANNOUNCER);
        }
    }

    // The classes of Android widgets tell services how to describe each role.
    @SuppressWarnings("deprecation")
    @Override
    protected void onPopulateNodeForVirtualView(int virtualViewId, @NonNull AccessibilityNodeInfoCompat node) {
        markAsked();

        if (virtualViewId == ANNOUNCER) {
            node.setClassName("android.widget.TextView");
            node.setBoundsInParent(new Rect(0, 0, view.getWidth(), view.getHeight()));
            node.setText(announcement);
            node.setLiveRegion(interrupting ? ViewCompat.ACCESSIBILITY_LIVE_REGION_ASSERTIVE : ViewCompat.ACCESSIBILITY_LIVE_REGION_POLITE);
            return;
        }

        // The helper still describes a virtual view that left the tree when it tells services that the view lost the accessibility focus.
        int index = indexOfVirtualView(virtualViewId);
        if (index < 0) {
            node.setBoundsInParent(new Rect());
            node.setContentDescription("");
            return;
        }

        Element element = elements[index];
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
            node.setExpandedState(AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL);
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
        int index = indexOfVirtualView(virtualViewId);
        if (index < 0) {
            return false;
        }

        Element element = elements[index];
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
        int index = indexOfVirtualView(virtualViewId);
        if (index < 0) {
            return;
        }

        Element element = elements[index];
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
