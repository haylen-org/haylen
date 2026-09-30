#include "platform/linux/LinuxGlib.hpp"

namespace haylen::platform {

void* LinuxGlib::library = nullptr;
bool LinuxGlib::opened = false;
int (*LinuxGlib::iterateContext)(void*, int) = nullptr;

void* LinuxGlib::open() {
    if (opened) {
        return library;
    }
    opened = true;
    void* loaded = dlopen("libgio-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    if (loaded != nullptr && find(loaded, iterateContext, "g_main_context_iteration")) {
        library = loaded;
    }
    return library;
}

void LinuxGlib::iterate() {
    if (iterateContext != nullptr) {
        iterateContext(nullptr, 0);
    }
}

} // namespace haylen::platform
