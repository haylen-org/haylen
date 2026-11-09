#include "platform/linux/LinuxLinks.hpp"

#include <algorithm>

#include "haylen/core/Log.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/linux/LinuxGlib.hpp"

namespace haylen::platform {

void* (*LinuxLinks::getBus)(int, void*, void**) = nullptr;
void* (*LinuxLinks::callSync)(void*, const char*, const char*, const char*, const char*, void*, const char*, int, int, void*, void**) = nullptr;
void* (*LinuxLinks::parseNode)(const char*, void**) = nullptr;
void* (*LinuxLinks::findInterface)(void*, const char*) = nullptr;
unsigned int (*LinuxLinks::registerObject)(void*, const char*, void*, const VTable*, void*, void (*)(void*), void**) = nullptr;
void (*LinuxLinks::returnValue)(void*, void*) = nullptr;
void* (*LinuxLinks::newStrings)(const char* const*, std::ptrdiff_t) = nullptr;
void* (*LinuxLinks::newString)(const char*) = nullptr;
void* (*LinuxLinks::newUint32)(std::uint32_t) = nullptr;
void* (*LinuxLinks::newParsed)(const char*, ...) = nullptr;
void* (*LinuxLinks::newTuple)(void* const*, std::size_t) = nullptr;
void* (*LinuxLinks::getChild)(void*, std::size_t) = nullptr;
std::size_t (*LinuxLinks::countChildren)(void*) = nullptr;
const char* (*LinuxLinks::getString)(void*, std::size_t*) = nullptr;
std::uint32_t (*LinuxLinks::getUint32)(void*) = nullptr;
void (*LinuxLinks::unref)(void*) = nullptr;
LinuxLinks::VTable LinuxLinks::table{.methodCall = &LinuxLinks::methodCalled};
void* LinuxLinks::bus = nullptr;

// The call never starts the app through the bus, so it reaches only an instance that runs.
bool LinuxLinks::forward(std::string_view identifier, std::string_view url) {
    void* connection = connect();
    if (connection == nullptr) {
        return false;
    }
    const std::string link(url);
    const std::array<const char*, 1> links{link.c_str()};
    const std::array<void*, 2> arguments{newStrings(links.data(), static_cast<std::ptrdiff_t>(links.size())), newParsed("@a{sv} {}")};
    void* reply = callSync(connection, std::string(identifier).c_str(), getPath(identifier).c_str(), kInterface, "Open", newTuple(arguments.data(), arguments.size()), nullptr, kNoAutoStart, kTimeoutMilliseconds, nullptr, nullptr);
    if (reply == nullptr) {
        return false;
    }
    unref(reply);
    return true;
}

// The object exists before the name, so a call that comes as soon as the app owns the name finds it. The connection stays open for the process, which keeps the object and the name.
void LinuxLinks::listen(std::string_view identifier) {
    void* connection = connect();
    if (connection == nullptr) {
        core::Log::info("The app takes no links of later processes, because the session has no bus that GIO reaches.");
        return;
    }
    void* node = parseNode(kIntrospection, nullptr);
    if (registerObject(connection, getPath(identifier).c_str(), findInterface(node, kInterface), &table, nullptr, nullptr, nullptr) == 0) {
        core::Log::warning("The app takes no links of later processes, because the session bus refused the object of the app.");
        return;
    }

    const std::array<void*, 2> arguments{newString(std::string(identifier).c_str()), newUint32(kDoNotQueue)};
    void* reply = callSync(connection, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName", newTuple(arguments.data(), arguments.size()), "(u)", 0, kTimeoutMilliseconds, nullptr, nullptr);
    if (reply == nullptr) {
        core::Log::warning("The app takes no links of later processes, because the session bus did not give it the name \"{}\".", identifier);
        return;
    }
    void* answer = getChild(reply, 0);
    const std::uint32_t owned = getUint32(answer);
    unref(answer);
    unref(reply);
    if (owned != kPrimaryOwner) {
        core::Log::warning("The app takes no links of later processes, because another instance of it owns the name \"{}\" on the session bus.", identifier);
    }
}

// The session bus opens once and stays open for the process.
void* LinuxLinks::connect() {
    if (bus != nullptr) {
        return bus;
    }
    void* library = LinuxGlib::open();
    if (library == nullptr || !load(library)) {
        return nullptr;
    }
    bus = getBus(kSessionBus, nullptr, nullptr);
    return bus;
}

bool LinuxLinks::load(void* library) {
    return LinuxGlib::find(library, getBus, "g_bus_get_sync") && LinuxGlib::find(library, callSync, "g_dbus_connection_call_sync") && LinuxGlib::find(library, parseNode, "g_dbus_node_info_new_for_xml") && LinuxGlib::find(library, findInterface, "g_dbus_node_info_lookup_interface") && LinuxGlib::find(library, registerObject, "g_dbus_connection_register_object") && LinuxGlib::find(library, returnValue, "g_dbus_method_invocation_return_value") && LinuxGlib::find(library, newStrings, "g_variant_new_strv") && LinuxGlib::find(library, newString, "g_variant_new_string") && LinuxGlib::find(library, newUint32, "g_variant_new_uint32") && LinuxGlib::find(library, newParsed, "g_variant_new_parsed") && LinuxGlib::find(library, newTuple, "g_variant_new_tuple") && LinuxGlib::find(library, getChild, "g_variant_get_child_value") && LinuxGlib::find(library, countChildren, "g_variant_n_children") && LinuxGlib::find(library, getString, "g_variant_get_string") && LinuxGlib::find(library, getUint32, "g_variant_get_uint32") && LinuxGlib::find(library, unref, "g_variant_unref");
}

std::string LinuxLinks::getPath(std::string_view identifier) {
    std::string path = "/" + std::string(identifier);
    std::ranges::replace(path, '.', '/');
    std::ranges::replace(path, '-', '_');
    return path;
}

// Every link of `Open` reaches the app, while `Activate` and `ActivateAction` bring nothing the app hears.
void LinuxLinks::methodCalled(void*, const char*, const char*, const char*, const char* method, void* parameters, void* invocation, void*) {
    if (std::string_view(method) == "Open") {
        void* links = getChild(parameters, 0);
        for (std::size_t index = 0, count = countChildren(links); index < count; ++index) {
            void* link = getChild(links, index);
            LinkRelay::receive(getString(link, nullptr));
            unref(link);
        }
        unref(links);
    }
    returnValue(invocation, nullptr);
}

} // namespace haylen::platform
