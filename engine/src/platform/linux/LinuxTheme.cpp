#include "platform/linux/LinuxTheme.hpp"

#include <array>
#include <string_view>

#include "haylen/platform/Theme.hpp"
#include "platform/SystemState.hpp"
#include "platform/linux/LinuxGlib.hpp"
#include "platform/sokol/SokolHost.hpp"

namespace haylen::platform {

void* (*LinuxTheme::busGetSync)(int, void*, void**) = nullptr;
void* (*LinuxTheme::callSync)(void*, const char*, const char*, const char*, const char*, void*, const char*, int, int, void*, void**) = nullptr;
unsigned int (*LinuxTheme::subscribe)(void*, const char*, const char*, const char*, const char*, const char*, int, SignalCallback, void*, void (*)(void*)) = nullptr;
void* (*LinuxTheme::newString)(const char*) = nullptr;
void* (*LinuxTheme::newTuple)(void* const*, std::size_t) = nullptr;
void* (*LinuxTheme::getChild)(void*, std::size_t) = nullptr;
int (*LinuxTheme::isOfType)(void*, const char*) = nullptr;
void* (*LinuxTheme::getVariant)(void*) = nullptr;
std::uint32_t (*LinuxTheme::getUint32)(void*) = nullptr;
const char* (*LinuxTheme::getString)(void*, std::size_t*) = nullptr;
void (*LinuxTheme::unref)(void*) = nullptr;

// The subscription comes before the first read, so a change in between is never lost. The connection stays open for the process, which keeps the subscription.
void LinuxTheme::observe() {
    void* library = LinuxGlib::open();
    if (library == nullptr || !load(library)) {
        return;
    }
    void* connection = busGetSync(kSessionBus, nullptr, nullptr);
    if (connection == nullptr) {
        return;
    }
    subscribe(connection, kPortal, kSettings, "SettingChanged", kPortalPath, kAppearance, 0, settingChanged, nullptr, nullptr);

    const std::array<void*, 2> arguments{newString(kAppearance), newString(kColorScheme)};
    void* reply = callSync(connection, kPortal, kPortalPath, kSettings, "Read", newTuple(arguments.data(), arguments.size()), "(v)", 0, kTimeoutMilliseconds, nullptr, nullptr);
    if (reply == nullptr) {
        return;
    }
    void* value = getChild(reply, 0);
    report(value);
    unref(value);
    unref(reply);
}

bool LinuxTheme::load(void* library) {
    return LinuxGlib::find(library, busGetSync, "g_bus_get_sync") && LinuxGlib::find(library, callSync, "g_dbus_connection_call_sync") && LinuxGlib::find(library, subscribe, "g_dbus_connection_signal_subscribe") && LinuxGlib::find(library, newString, "g_variant_new_string") && LinuxGlib::find(library, newTuple, "g_variant_new_tuple") && LinuxGlib::find(library, getChild, "g_variant_get_child_value") && LinuxGlib::find(library, isOfType, "g_variant_is_of_type") && LinuxGlib::find(library, getVariant, "g_variant_get_variant") && LinuxGlib::find(library, getUint32, "g_variant_get_uint32") && LinuxGlib::find(library, getString, "g_variant_get_string") && LinuxGlib::find(library, unref, "g_variant_unref");
}

// Takes the variant that holds the color scheme, which the first version of the portal wraps in one more variant. The scheme is 1 when the user prefers dark colors, 2 for light ones and 0 without a preference.
void LinuxTheme::report(void* variant) {
    void* value = getVariant(variant);
    while (isOfType(value, "v") != 0) {
        void* inner = getVariant(value);
        unref(value);
        value = inner;
    }
    if (isOfType(value, "u") != 0) {
        SokolHost::getSystemState().setTheme(getUint32(value) == kPreferDark ? Theme::Dark : Theme::Light);
    }
    unref(value);
}

// The portal signals every setting of the appearance namespace with its key and its new value.
void LinuxTheme::settingChanged(void*, const char*, const char*, const char*, const char*, void* parameters, void*) {
    void* key = getChild(parameters, 1);
    const bool scheme = std::string_view(getString(key, nullptr)) == kColorScheme;
    unref(key);
    if (!scheme) {
        return;
    }
    void* value = getChild(parameters, 2);
    report(value);
    unref(value);
}

} // namespace haylen::platform
