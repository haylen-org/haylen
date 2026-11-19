package dev.haylen;

import static org.junit.Assert.assertEquals;

import java.util.ArrayList;
import java.util.List;
import org.junit.Test;

public class HaylenPermissionQueueTest {
    // Prompts that show every request and write down what happened to each one.
    private static class RecordedPrompts implements HaylenPermissionQueue.Prompts<String> {
        final List<String> log = new ArrayList<>();

        @Override
        public boolean show(String request) {
            log.add("show " + request);
            return true;
        }

        @Override
        public void answerPrompted(String request) {
            log.add("prompted " + request);
        }

        @Override
        public void answerWithoutPrompt(String request) {
            log.add("held " + request);
        }
    }

    @Test
    public void showsOnePromptAtATimeInOrder() {
        RecordedPrompts prompts = new RecordedPrompts();
        HaylenPermissionQueue<String> queue = new HaylenPermissionQueue<>(prompts);

        queue.add("camera");
        queue.add("location");
        queue.answered();
        queue.answered();

        assertEquals(List.of("show camera", "prompted camera", "show location", "prompted location"), prompts.log);
    }

    // An activity that finished for good while its prompt showed never hears the answer, so the next activity answers the waiting requests, and later requests show their prompts again.
    @Test
    public void answersTheRequestsOfAnActivityThatFinishedDuringItsPrompt() {
        RecordedPrompts prompts = new RecordedPrompts();
        HaylenPermissionQueue<String> queue = new HaylenPermissionQueue<>(prompts);

        queue.add("camera");
        queue.add("location");
        queue.answerWithoutPrompts();
        queue.answered();
        queue.add("microphone");

        assertEquals(List.of("show camera", "held camera", "held location", "show microphone"), prompts.log);
    }

    @Test
    public void movesOnFromARequestThatCouldNotShowItsPrompt() {
        RecordedPrompts prompts = new RecordedPrompts() {
            @Override
            public boolean show(String request) {
                super.show(request);
                return !request.equals("camera");
            }
        };
        HaylenPermissionQueue<String> queue = new HaylenPermissionQueue<>(prompts);

        queue.add("camera");
        queue.add("location");
        queue.add("microphone");

        assertEquals(List.of("show camera", "show location"), prompts.log);
    }
}
