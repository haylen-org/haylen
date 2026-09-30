#pragma once

#include <dlfcn.h>

namespace haylen::platform {

// GLib, GObject and GIO of the desktop, which the engine loads at run time through GIO, whose dependencies bring the other two, so apps also run on systems without them. The engine runs the main context of GLib once per frame without blocking, where the change signals of the settings portal and the answers of GTK dialogs reach the frame thread.
class LinuxGlib final {
  public:
    // Loads GIO the first time and returns its handle, which is null on systems without it.
    [[nodiscard]] static void* open();

    // Sets the function to the symbol that a library or one of its dependencies defines, and returns whether the symbol exists.
    template <typename Function> static bool find(void* library, Function& function, const char* name) {
        function = reinterpret_cast<Function>(dlsym(library, name));
        return function != nullptr;
    }

    // Dispatches what is ready in the main context without waiting, once GIO is loaded.
    static void iterate();

  private:
    static void* library;
    static bool opened;
    static int (*iterateContext)(void* context, int mayBlock);
};

} // namespace haylen::platform
