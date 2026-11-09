#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace haylen::platform {

// Hands the links that the desktop opens to the instance of the app that runs, through the interface `org.freedesktop.Application` on the session bus, which GIO reaches when the engine loads it at run time. Desktops start the executable of the desktop entry for every link of a scheme that its `MimeType` names, with the link after `--link`, so the new process calls `Open` on the bus name of the app, which is its identifier, and the instance that owns the name reports the link to `LinkRelay` while the main context of GLib runs. Without GIO or a session bus every process keeps its own link.
class LinuxLinks final {
  public:
    [[nodiscard]] static bool forward(std::string_view identifier, std::string_view url);
    static void listen(std::string_view identifier);

  private:
    static constexpr int kSessionBus = 2;
    static constexpr int kNoAutoStart = 1;
    static constexpr int kTimeoutMilliseconds = 2000;
    static constexpr std::uint32_t kDoNotQueue = 4;
    static constexpr std::uint32_t kPrimaryOwner = 1;
    static constexpr const char* kInterface = "org.freedesktop.Application";
    static constexpr const char* kIntrospection = "<node><interface name='org.freedesktop.Application'><method name='Activate'><arg type='a{sv}' direction='in'/></method><method name='Open'><arg type='as' direction='in'/><arg type='a{sv}' direction='in'/></method><method name='ActivateAction'><arg type='s' direction='in'/><arg type='av' direction='in'/><arg type='a{sv}' direction='in'/></method></interface></node>";

    using MethodCall = void (*)(void* connection, const char* sender, const char* path, const char* interface, const char* method, void* parameters, void* invocation, void* data);

    // The table of `GDBusInterfaceVTable`, whose padding GIO keeps for later entries.
    struct VTable {
        MethodCall methodCall = nullptr;
        void* getProperty = nullptr;
        void* setProperty = nullptr;
        std::array<void*, 8> padding{};
    };

    static void* (*getBus)(int type, void* cancellable, void** error);
    static void* (*callSync)(void* connection, const char* name, const char* path, const char* interface, const char* method, void* parameters, const char* replyType, int flags, int timeout, void* cancellable, void** error);
    static void* (*parseNode)(const char* xml, void** error);
    static void* (*findInterface)(void* node, const char* name);
    static unsigned int (*registerObject)(void* connection, const char* path, void* interface, const VTable* table, void* data, void (*release)(void*), void** error);
    static void (*returnValue)(void* invocation, void* value);
    static void* (*newStrings)(const char* const* strings, std::ptrdiff_t length);
    static void* (*newString)(const char* text);
    static void* (*newUint32)(std::uint32_t value);
    static void* (*newParsed)(const char* text, ...);
    static void* (*newTuple)(void* const* children, std::size_t count);
    static void* (*getChild)(void* variant, std::size_t index);
    static std::size_t (*countChildren)(void* variant);
    static const char* (*getString)(void* variant, std::size_t* length);
    static std::uint32_t (*getUint32)(void* variant);
    static void (*unref)(void* variant);
    static VTable table;
    static void* bus;

    // Loads GIO and returns the session bus, or null without GIO or a session bus.
    [[nodiscard]] static void* connect();
    [[nodiscard]] static bool load(void* library);

    // The object path of the app, its identifier with slashes for the dots and underscores for the dashes.
    [[nodiscard]] static std::string getPath(std::string_view identifier);
    static void methodCalled(void* connection, const char* sender, const char* path, const char* interface, const char* method, void* parameters, void* invocation, void* data);
};

} // namespace haylen::platform
