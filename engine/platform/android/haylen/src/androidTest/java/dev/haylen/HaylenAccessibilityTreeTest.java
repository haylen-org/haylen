package dev.haylen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.app.Instrumentation;
import android.view.View;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class HaylenAccessibilityTreeTest {
    // The tree tells the engine library that a service reads it, so the library that holds its natives loads first.
    @BeforeClass
    public static void loadLibrary() {
        System.loadLibrary("haylen");
    }

    // The engine has no action that collapses an element, so an expanded element tells its state and offers no collapse that would fail.
    @Test
    public void reportsAnExpandedElementWithoutACollapseAction() {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        AtomicReference<View> view = new AtomicReference<>();
        AtomicReference<HaylenAccessibilityTree> tree = new AtomicReference<>();
        instrumentation.runOnMainSync(() -> {
            view.set(new View(instrumentation.getTargetContext()));
            tree.set(HaylenAccessibilityTree.attach(view.get()));
        });

        String json = "{\"focused\": 0, \"elements\": [{\"id\": 7, \"role\": \"button\", \"label\": \"Options\", \"value\": \"\", \"hint\": \"\", \"bounds\": [0, 0, 40, 20], \"states\": 16, \"actions\": 2}]}";
        HaylenAccessibilityTree.update(json.getBytes(StandardCharsets.UTF_8));
        instrumentation.waitForIdleSync();

        AtomicReference<AccessibilityNodeInfoCompat> node = new AtomicReference<>();
        instrumentation.runOnMainSync(() -> node.set(tree.get().getAccessibilityNodeProvider(view.get()).createAccessibilityNodeInfo(0)));

        assertEquals(AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL, node.get().getExpandedState());
        assertFalse(node.get().getActionList().contains(AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COLLAPSE));
    }
}
