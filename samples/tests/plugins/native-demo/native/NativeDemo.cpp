#include "NativeDemoPlugin.hpp"

#if defined(_WIN32)
#define NATIVE_DEMO_EXPORT __declspec(dllexport)
#else
#define NATIVE_DEMO_EXPORT __attribute__((visibility("default")))
#endif

// The init function that `native.load('native-demo', {init = 'native_demo_haylen_init'})` finds by its C name, which forwards to the plugin.
extern "C" NATIVE_DEMO_EXPORT int native_demo_haylen_init(const HaylenNativeApi* api) {
    return NativeDemoPlugin::initialize(api);
}
