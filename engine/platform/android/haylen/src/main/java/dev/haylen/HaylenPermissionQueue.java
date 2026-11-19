package dev.haylen;

import java.util.ArrayDeque;

// The requests of permissions that wait for the prompt of the system, which shows one at a time, in the order they came. Only the main thread reaches it.
final class HaylenPermissionQueue<R> {
    // What the queue asks of the permissions it holds.
    interface Prompts<R> {
        // Shows the prompt of the request and returns `true`, or answers the request at once and returns `false` when no prompt can show.
        boolean show(R request);

        // Answers the request whose prompt the person closed.
        void answerPrompted(R request);

        // Answers the request with the state the app holds, without a prompt.
        void answerWithoutPrompt(R request);
    }

    private final ArrayDeque<R> waiting = new ArrayDeque<>();
    private final Prompts<R> prompts;
    private boolean showing;

    HaylenPermissionQueue(Prompts<R> prompts) {
        this.prompts = prompts;
    }

    void add(R request) {
        waiting.add(request);
        if (waiting.size() == 1) {
            showNext();
        }
    }

    // The prompt that shows was answered. A result that Android delivers while no prompt shows, such as one that reached a new process, answers nothing.
    void answered() {
        R head = waiting.peek();
        if (!showing || head == null) {
            return;
        }

        showing = false;
        prompts.answerPrompted(head);
        waiting.poll();
        showNext();
    }

    // Android never answers the prompt of an activity that finished for good, so its requests and the ones behind them answer what the app holds, and the next request shows its own prompt.
    void answerWithoutPrompts() {
        showing = false;
        for (R request = waiting.peek(); request != null; request = waiting.peek()) {
            prompts.answerWithoutPrompt(request);
            waiting.poll();
        }
    }

    // A request stays at the head while it answers, so a request that its answer adds waits behind it.
    private void showNext() {
        while (!waiting.isEmpty()) {
            if (prompts.show(waiting.peek())) {
                showing = true;
                return;
            }

            waiting.poll();
        }
    }
}
