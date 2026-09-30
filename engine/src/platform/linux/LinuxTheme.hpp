#pragma once

#include <cstddef>
#include <cstdint>

namespace haylen::platform {

// Follows the color scheme of the desktop through the settings portal, which the engine reaches with GIO loaded at run time. Without GIO, a session bus or a portal that knows the color scheme, the theme stays light and never changes.
class LinuxTheme final {
  public:
    // Reads the color scheme once, waiting a second at most for the portal, and subscribes to its changes, which arrive while the main context of GLib runs.
    static void observe();

  private:
    static constexpr int kSessionBus = 2;
    static constexpr int kTimeoutMilliseconds = 1000;
    static constexpr std::uint32_t kPreferDark = 1;
    static constexpr const char* kPortal = "org.freedesktop.portal.Desktop";
    static constexpr const char* kPortalPath = "/org/freedesktop/portal/desktop";
    static constexpr const char* kSettings = "org.freedesktop.portal.Settings";
    static constexpr const char* kAppearance = "org.freedesktop.appearance";
    static constexpr const char* kColorScheme = "color-scheme";

    using SignalCallback = void (*)(void* connection, const char* sender, const char* path, const char* interface, const char* signal, void* parameters, void* data);

    static void* (*busGetSync)(int type, void* cancellable, void** error);
    static void* (*callSync)(void* connection, const char* name, const char* path, const char* interface, const char* method, void* parameters, const char* replyType, int flags, int timeout, void* cancellable, void** error);
    static unsigned int (*subscribe)(void* connection, const char* sender, const char* interface, const char* member, const char* path, const char* argument, int flags, SignalCallback callback, void* data, void (*release)(void*));
    static void* (*newString)(const char* text);
    static void* (*newTuple)(void* const* children, std::size_t count);
    static void* (*getChild)(void* variant, std::size_t index);
    static int (*isOfType)(void* variant, const char* type);
    static void* (*getVariant)(void* variant);
    static std::uint32_t (*getUint32)(void* variant);
    static const char* (*getString)(void* variant, std::size_t* length);
    static void (*unref)(void* variant);

    [[nodiscard]] static bool load(void* library);
    static void report(void* variant);
    static void settingChanged(void* connection, const char* sender, const char* path, const char* interface, const char* signal, void* parameters, void* data);
};

} // namespace haylen::platform
