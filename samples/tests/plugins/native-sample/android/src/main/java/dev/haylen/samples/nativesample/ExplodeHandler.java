package dev.haylen.samples.nativesample;

import dev.haylen.HaylenBridge;

// Answers `native-sample.explode` by throwing, which fails the call with the code `exception` instead of crashing the app.
final class ExplodeHandler implements HaylenBridge.MethodHandler {
    @Override
    public void handle(Object params, HaylenBridge.Reply reply) {
        throw new IllegalStateException("Java threw on purpose.");
    }
}
