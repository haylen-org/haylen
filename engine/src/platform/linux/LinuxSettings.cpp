#include "platform/linux/LinuxSettings.hpp"

#include <array>
#include <string>

#include "haylen/platform/Theme.hpp"
#include "platform/SystemState.hpp"
#include "platform/linux/LinuxGlib.hpp"
#include "platform/sokol/SokolHost.hpp"

namespace haylen::platform {

void* (*LinuxSettings::busGetSync)(int, void*, void**) = nullptr;
void* (*LinuxSettings::callSync)(void*, const char*, const char*, const char*, const char*, void*, const char*, int, int, void*, void**) = nullptr;
unsigned int (*LinuxSettings::subscribe)(void*, const char*, const char*, const char*, const char*, const char*, int, SignalCallback, void*, void (*)(void*)) = nullptr;
void* (*LinuxSettings::newString)(const char*) = nullptr;
void* (*LinuxSettings::newTuple)(void* const*, std::size_t) = nullptr;
void* (*LinuxSettings::getChild)(void*, std::size_t) = nullptr;
int (*LinuxSettings::isOfType)(void*, const char*) = nullptr;
void* (*LinuxSettings::getVariant)(void*) = nullptr;
std::uint32_t (*LinuxSettings::getUint32)(void*) = nullptr;
double (*LinuxSettings::getDouble)(void*) = nullptr;
const char* (*LinuxSettings::getString)(void*, std::size_t*) = nullptr;
void (*LinuxSettings::unref)(void*) = nullptr;
AccessibilitySettings LinuxSettings::accessibility;

// The subscription comes before the first reads, so a change in between is never lost. The connection stays open for the process, which keeps the subscription.
void LinuxSettings::observe() {
    void* library = LinuxGlib::open();
    if (library == nullptr || !load(library)) {
        return;
    }
    void* connection = busGetSync(kSessionBus, nullptr, nullptr);
    if (connection == nullptr) {
        return;
    }
    subscribe(connection, kPortal, kSettings, "SettingChanged", kPortalPath, nullptr, 0, settingChanged, nullptr, nullptr);
    read(connection, kAppearance, kColorScheme);
    read(connection, kAppearance, kContrast);
    read(connection, kAppearance, kMotion);
    read(connection, kInterface, kTextScaling);
}

void LinuxSettings::setScreenReader(bool value) {
    accessibility.screenReader = value;
    SokolHost::getSystemState().setAccessibility(accessibility);
}

bool LinuxSettings::load(void* library) {
    return LinuxGlib::find(library, busGetSync, "g_bus_get_sync") && LinuxGlib::find(library, callSync, "g_dbus_connection_call_sync") && LinuxGlib::find(library, subscribe, "g_dbus_connection_signal_subscribe") && LinuxGlib::find(library, newString, "g_variant_new_string") && LinuxGlib::find(library, newTuple, "g_variant_new_tuple") && LinuxGlib::find(library, getChild, "g_variant_get_child_value") && LinuxGlib::find(library, isOfType, "g_variant_is_of_type") && LinuxGlib::find(library, getVariant, "g_variant_get_variant") && LinuxGlib::find(library, getUint32, "g_variant_get_uint32") && LinuxGlib::find(library, getDouble, "g_variant_get_double") && LinuxGlib::find(library, getString, "g_variant_get_string") && LinuxGlib::find(library, unref, "g_variant_unref");
}

void LinuxSettings::read(void* connection, std::string_view space, std::string_view key) {
    const std::array<void*, 2> arguments{newString(std::string(space).c_str()), newString(std::string(key).c_str())};
    void* reply = callSync(connection, kPortal, kPortalPath, kSettings, "Read", newTuple(arguments.data(), arguments.size()), "(v)", 0, kTimeoutMilliseconds, nullptr, nullptr);
    if (reply == nullptr) {
        return;
    }
    void* value = getChild(reply, 0);
    apply(space, key, value);
    unref(value);
    unref(reply);
}

// Takes the variant that holds the value, which the first version of the portal wraps in one more variant. The color scheme is 1 when the user prefers dark colors, the contrast is 1 for higher contrast and the reduced motion is 1 when the user wants less motion.
void LinuxSettings::apply(std::string_view space, std::string_view key, void* variant) {
    void* value = getVariant(variant);
    while (isOfType(value, "v") != 0) {
        void* inner = getVariant(value);
        unref(value);
        value = inner;
    }
    const bool number = isOfType(value, "u") != 0;
    if (space == kAppearance && key == kColorScheme && number) {
        SokolHost::getSystemState().setTheme(getUint32(value) == kPreferDark ? Theme::Dark : Theme::Light);
    } else if (space == kAppearance && key == kContrast && number) {
        accessibility.highContrast = getUint32(value) == kHigherContrast;
        SokolHost::getSystemState().setAccessibility(accessibility);
    } else if (space == kAppearance && key == kMotion && number) {
        accessibility.reducedMotion = getUint32(value) == kReducedMotion;
        SokolHost::getSystemState().setAccessibility(accessibility);
    } else if (space == kInterface && key == kTextScaling && isOfType(value, "d") != 0) {
        accessibility.textScale = static_cast<float>(getDouble(value));
        SokolHost::getSystemState().setAccessibility(accessibility);
    }
    unref(value);
}

// The portal signals every setting with its namespace, its key and its new value.
void LinuxSettings::settingChanged(void*, const char*, const char*, const char*, const char*, void* parameters, void*) {
    void* space = getChild(parameters, 0);
    void* key = getChild(parameters, 1);
    void* value = getChild(parameters, 2);
    apply(getString(space, nullptr), getString(key, nullptr), value);
    unref(value);
    unref(key);
    unref(space);
}

} // namespace haylen::platform
