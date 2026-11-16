#include "{{NAME}}Plugin.hpp"

#if defined(_WIN32)
#define {{MACRO}}_EXPORT __declspec(dllexport)
#else
#define {{MACRO}}_EXPORT __attribute__((visibility("default")))
#endif

// The init function that `native.load('{{ID}}', {init = '{{SYMBOL}}_haylen_init'})` finds by its C name, which forwards to the plugin.
extern "C" {{MACRO}}_EXPORT int {{SYMBOL}}_haylen_init(const HaylenNativeApi* api) {
    return {{NAME}}Plugin::initialize(api);
}
