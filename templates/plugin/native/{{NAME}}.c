// Desktop part of the {{TITLE}} plugin, a C library for macOS, Windows and Linux. The Lua API loads it with `native.load`, which hands its init function the `HaylenNativeApi` of the engine, through which the library answers the methods of the plugin and sends its events. Its handlers run on the frame thread, and the library may answer and send events from any thread.

#include "haylen/platform/native/HaylenNative.h"

#if defined(_WIN32)
#define {{MACRO}}_EXPORT __declspec(dllexport)
#else
#define {{MACRO}}_EXPORT __attribute__((visibility("default")))
#endif

static const HaylenNativeApi* {{SYMBOL}}_api = 0;

// Answers `{{ID}}.echo` with the parameters it receives, which hold the message, and sends them to the app again as the `{{ID}}.echoed` event.
static void {{SYMBOL}}_echo(void* user, uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    (void)user;
    (void)method;
    (void)buffers;
    (void)bufferCount;
    {{SYMBOL}}_api->emit("{{ID}}.echoed", paramsJson, 0, 0, 0);
    {{SYMBOL}}_api->resolve(call, 1, paramsJson, 0, 0);
}

// Registers the methods of the plugin and declares the library the native part of `{{ID}}`.
{{MACRO}}_EXPORT int {{SYMBOL}}_haylen_init(const HaylenNativeApi* api) {
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    {{SYMBOL}}_api = api;
    api->registerHandler("{{ID}}.echo", {{SYMBOL}}_echo, 0, 0);
    api->registerPlugin("{{ID}}");
    return 0;
}
