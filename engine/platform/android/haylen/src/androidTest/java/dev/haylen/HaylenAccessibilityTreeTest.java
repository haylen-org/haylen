package dev.haylen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.app.Instrumentation;
import android.graphics.Rect;
import android.view.View;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class HaylenAccessibilityTreeTest {
    private final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
    private View view;
    private HaylenAccessibilityTree tree;

    // The tree tells the engine library that a service reads it, so the library that holds its natives loads first.
    @BeforeClass
    public static void loadLibrary() {
        System.loadLibrary("haylen");
    }

    @Before
    public void attach() {
        instrumentation.runOnMainSync(() -> {
            view = new View(instrumentation.getTargetContext());
            tree = HaylenAccessibilityTree.attach(view);
        });
    }

    @After
    public void detach() {
        instrumentation.runOnMainSync(() -> tree.detach());
    }

    // The engine has no action that collapses an element, so an expanded element tells its state and offers no collapse that would fail.
    @Test
    public void reportsAnExpandedElementWithoutACollapseAction() {
        update(element(7, "Options", 0, 16));

        AccessibilityNodeInfoCompat node = node(virtualViewOf("Options"));
        assertNotNull(node);
        assertEquals(AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL, node.getExpandedState());
        assertFalse(node.getActionList().contains(AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COLLAPSE));
    }

    // A node that a service holds, such as the button that a UI test found, describes its element while elements before it come and go, as the node of a view does, and once its element leaves the tree the node is gone instead of describing the element that took its place.
    @Test
    public void keepsTheNodeOfAnElementWhileElementsBeforeItComeAndGo() {
        update(element(7, "Start the camera", 0, 0), element(8, "Start the front camera", 60, 0));
        int camera = virtualViewOf("Start the camera");

        update(element(5, "Checks", 0, 0), element(7, "Start the camera", 60, 0), element(8, "Start the front camera", 120, 0));
        AccessibilityNodeInfoCompat moved = node(camera);
        assertNotNull(moved);
        assertEquals("Start the camera", moved.getContentDescription().toString());
        Rect bounds = new Rect();
        moved.getBoundsInParent(bounds);
        assertEquals(60, bounds.top);

        update(element(5, "Checks", 0, 0), element(8, "Start the front camera", 60, 0));
        assertNull(node(camera));
    }

    private static String element(long id, String label, int top, int states) {
        return String.format("{\"id\": %d, \"role\": \"button\", \"label\": \"%s\", \"value\": \"\", \"hint\": \"\", \"bounds\": [0, %d, 200, 40], \"states\": %d, \"actions\": 2}", id, label, top, states);
    }

    private void update(String... elements) {
        String json = "{\"focused\": 0, \"elements\": [" + String.join(", ", elements) + "]}";
        HaylenAccessibilityTree.update(json.getBytes(StandardCharsets.UTF_8));
        instrumentation.waitForIdleSync();
    }

    private int virtualViewOf(String label) {
        AtomicReference<Integer> found = new AtomicReference<>();
        instrumentation.runOnMainSync(() -> {
            List<Integer> ids = new ArrayList<>();
            tree.getVisibleVirtualViews(ids);
            for (int id : ids) {
                AccessibilityNodeInfoCompat node = tree.getAccessibilityNodeProvider(view).createAccessibilityNodeInfo(id);
                if (node != null && label.contentEquals(node.getContentDescription())) {
                    found.set(id);
                }
            }
        });
        assertNotNull("No node of the tree reads \"" + label + "\".", found.get());
        return found.get();
    }

    private AccessibilityNodeInfoCompat node(int virtualViewId) {
        AtomicReference<AccessibilityNodeInfoCompat> node = new AtomicReference<>();
        instrumentation.runOnMainSync(() -> node.set(tree.getAccessibilityNodeProvider(view).createAccessibilityNodeInfo(virtualViewId)));
        return node.get();
    }
}
