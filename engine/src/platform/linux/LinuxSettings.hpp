#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "haylen/platform/AccessibilitySettings.hpp"

namespace haylen::platform {

// Follows the color scheme and the accessibility settings of the desktop through the settings portal, which the engine reaches with GIO loaded at run time: the color scheme, the contrast and the reduced motion of the appearance namespace and the text scaling factor of the GNOME interface settings. Without GIO, a session bus or a portal that knows a setting, it keeps its default and never changes. Linux has no setting for reduced transparency, and the portal tells nothing about screen readers.
class LinuxSettings final {
  public:
    // Reads the settings once, waiting a second at most for the portal on each, and subscribes to their changes, which arrive while the main context of GLib runs.
    static void observe();

  private:
    static constexpr int kSessionBus = 2;
    static constexpr int kTimeoutMilliseconds = 1000;
    static constexpr std::uint32_t kPreferDark = 1;
    static constexpr std::uint32_t kHigherContrast = 1;
    static constexpr std::uint32_t kReducedMotion = 1;
    static constexpr const char* kPortal = "org.freedesktop.portal.Desktop";
    static constexpr const char* kPortalPath = "/org/freedesktop/portal/desktop";
    static constexpr const char* kSettings = "org.freedesktop.portal.Settings";
    static constexpr std::string_view kAppearance = "org.freedesktop.appearance";
    static constexpr std::string_view kInterface = "org.gnome.desktop.interface";
    static constexpr std::string_view kColorScheme = "color-scheme";
    static constexpr std::string_view kContrast = "contrast";
    static constexpr std::string_view kMotion = "reduced-motion";
    static constexpr std::string_view kTextScaling = "text-scaling-factor";

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
    static double (*getDouble)(void* variant);
    static const char* (*getString)(void* variant, std::size_t* length);
    static void (*unref)(void* variant);

    // The accessibility settings as the portal last told them, which each change updates one at a time.
    static AccessibilitySettings accessibility;

    [[nodiscard]] static bool load(void* library);
    static void read(void* connection, std::string_view space, std::string_view key);
    static void apply(std::string_view space, std::string_view key, void* variant);
    static void settingChanged(void* connection, const char* sender, const char* path, const char* interface, const char* signal, void* parameters, void* data);
};

} // namespace haylen::platform
