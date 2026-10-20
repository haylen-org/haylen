#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/platform/AccessibilityElement.hpp"

namespace haylen::platform {

// Serves the tree of accessible elements to the screen readers of Linux, such as Orca, over AT-SPI, the protocol of the accessibility bus of D-Bus, which the engine reaches with the GDBus of GIO loaded at run time. The app is an object of the bus with one frame, its window, whose children are the elements of the UI, each with the interfaces of its role: every element is accessible and a component, the ones with actions take them, and sliders and progress bars give their value. The connection, the method calls of screen readers and the events the engine sends all run on the frame thread, in the main context of GLib that the frame iterates. Without GIO or an accessibility bus nothing happens.
class LinuxAccessibility final {
  public:
    // Connects to the accessibility bus in the background, embeds the app in its registry and follows whether assistive technologies run.
    static void start();

    // Whether an assistive technology runs, as the bus reports, or asked for the elements of the app.
    [[nodiscard]] static bool isWanted() noexcept;

    // Takes the tree of a frame and tells the assistive technologies what changed in it.
    static void update(std::span<const AccessibilityElement> elements, std::uint64_t focused);

    // Has screen readers say a text, politely or at once.
    static void announce(std::string_view text, bool interrupt);

    // Tells screen readers that the window gained or lost the focus of the desktop.
    static void setActive(bool value);

  private:
    using ReadyCallback = void (*)(void* source, void* result, void* data);
    using MethodCall = void (*)(void* connection, const char* sender, const char* path, const char* interface, const char* method, void* parameters, void* invocation, void* data);
    using GetProperty = void* (*)(void* connection, const char* sender, const char* path, const char* interface, const char* property, void** error, void* data);
    using SetProperty = int (*)(void* connection, const char* sender, const char* path, const char* interface, const char* property, void* value, void** error, void* data);
    using Enumerate = char** (*)(void* connection, const char* sender, const char* path, void* data);
    using Introspect = void** (*)(void* connection, const char* sender, const char* path, const char* node, void* data);
    using Dispatch = const void* (*)(void* connection, const char* sender, const char* path, const char* interface, const char* node, void** out, void* data);
    using SignalCallback = void (*)(void* connection, const char* sender, const char* path, const char* interface, const char* signal, void* parameters, void* data);

    // The tables of GDBus, as `GDBusInterfaceVTable` and `GDBusSubtreeVTable` lay them out.
    struct InterfaceTable {
        MethodCall method = nullptr;
        GetProperty get = nullptr;
        SetProperty set = nullptr;
        std::array<void*, 8> padding{};
    };
    struct SubtreeTable {
        Enumerate enumerate = nullptr;
        Introspect introspect = nullptr;
        Dispatch dispatch = nullptr;
        std::array<void*, 8> padding{};
    };

    // An object of the app: its application root, its window or an element of the UI.
    struct Object {
        enum class Kind : std::uint8_t {
            None,
            Root,
            Window,
            Element,
        };

        Kind kind = Kind::None;
        std::uint64_t id = 0;
    };

    static constexpr int kSessionBus = 2;
    static constexpr int kConnectionFlags = 9;
    static constexpr int kDispatchToUnenumerated = 1;
    static constexpr int kTimeoutMilliseconds = 5000;
    static constexpr const char* kPrefix = "/org/a11y/atspi/accessible";
    static constexpr const char* kRootPath = "/org/a11y/atspi/accessible/root";
    static constexpr const char* kWindowPath = "/org/a11y/atspi/accessible/window";
    static constexpr const char* kNullPath = "/org/a11y/atspi/null";
    static constexpr const char* kCachePath = "/org/a11y/atspi/cache";
    static constexpr const char* kCache = "org.a11y.atspi.Cache";
    static constexpr const char* kAccessible = "org.a11y.atspi.Accessible";
    static constexpr const char* kComponent = "org.a11y.atspi.Component";
    static constexpr const char* kAction = "org.a11y.atspi.Action";
    static constexpr const char* kValue = "org.a11y.atspi.Value";
    static constexpr const char* kApplication = "org.a11y.atspi.Application";
    static constexpr const char* kObjectEvents = "org.a11y.atspi.Event.Object";
    static constexpr const char* kFocusEvents = "org.a11y.atspi.Event.Focus";
    static constexpr const char* kWindowEvents = "org.a11y.atspi.Event.Window";

    // The roles and states of AT-SPI, as `atspi-constants.h` numbers them.
    static constexpr std::uint32_t kRoleApplication = 75;
    static constexpr std::uint32_t kRoleFrame = 23;
    static constexpr std::array<std::uint32_t, 19> kRoles{99, 29, 83, 27, 43, 7, 62, 99, 44, 38, 37, 51, 79, 11, 42, 31, 16, 2, 6};
    static constexpr std::array<std::string_view, 19> kRoleNames{"grouping", "label", "heading", "image", "push button", "check box", "toggle button", "grouping", "radio button", "page tab list", "page tab", "slider", "entry", "combo box", "progress bar", "list", "dialog", "alert", "canvas"};
    static constexpr std::uint32_t kStateActive = 1;
    static constexpr std::uint32_t kStateBusy = 3;
    static constexpr std::uint32_t kStateChecked = 4;
    static constexpr std::uint32_t kStateEditable = 7;
    static constexpr std::uint32_t kStateEnabled = 8;
    static constexpr std::uint32_t kStateExpandable = 9;
    static constexpr std::uint32_t kStateExpanded = 10;
    static constexpr std::uint32_t kStateFocusable = 11;
    static constexpr std::uint32_t kStateFocused = 12;
    static constexpr std::uint32_t kStateSelectable = 22;
    static constexpr std::uint32_t kStateSelected = 23;
    static constexpr std::uint32_t kStateSensitive = 24;
    static constexpr std::uint32_t kStateShowing = 25;
    static constexpr std::uint32_t kStateSingleLine = 26;
    static constexpr std::uint32_t kStateVisible = 30;
    static constexpr std::uint32_t kStateCheckable = 41;
    static constexpr int kPolite = 1;
    static constexpr int kAssertive = 2;
    static constexpr std::uint32_t kLayerWidget = 3;
    static constexpr std::uint32_t kLayerWindow = 7;

    // The states whose changes the engine announces, by their names in the events of AT-SPI.
    static constexpr std::array<std::pair<std::uint32_t, const char*>, 6> kAnnouncedStates{{{kStateChecked, "checked"}, {kStateSelected, "selected"}, {kStateExpanded, "expanded"}, {kStateBusy, "busy"}, {kStateEnabled, "enabled"}, {kStateSensitive, "sensitive"}}};

    // The actions a screen reader runs through the action interface, by their names.
    static constexpr std::array<std::pair<AccessibilityElement::Action, std::string_view>, 3> kActions{{{AccessibilityElement::Activate, "click"}, {AccessibilityElement::Increment, "increment"}, {AccessibilityElement::Decrement, "decrement"}}};

    static constexpr const char* kIntrospection = R"xml(<node>
<interface name="org.a11y.atspi.Accessible">
<property name="Name" type="s" access="read"/><property name="Description" type="s" access="read"/><property name="Parent" type="(so)" access="read"/><property name="ChildCount" type="i" access="read"/><property name="Locale" type="s" access="read"/><property name="AccessibleId" type="s" access="read"/><property name="HelpText" type="s" access="read"/>
<method name="GetChildAtIndex"><arg direction="in" name="index" type="i"/><arg direction="out" type="(so)"/></method>
<method name="GetChildren"><arg direction="out" type="a(so)"/></method>
<method name="GetIndexInParent"><arg direction="out" type="i"/></method>
<method name="GetRelationSet"><arg direction="out" type="a(ua(so))"/></method>
<method name="GetRole"><arg direction="out" type="u"/></method>
<method name="GetRoleName"><arg direction="out" type="s"/></method>
<method name="GetLocalizedRoleName"><arg direction="out" type="s"/></method>
<method name="GetState"><arg direction="out" type="au"/></method>
<method name="GetAttributes"><arg direction="out" type="a{ss}"/></method>
<method name="GetApplication"><arg direction="out" type="(so)"/></method>
<method name="GetInterfaces"><arg direction="out" type="as"/></method>
</interface>
<interface name="org.a11y.atspi.Component">
<method name="Contains"><arg direction="in" name="x" type="i"/><arg direction="in" name="y" type="i"/><arg direction="in" name="coordType" type="u"/><arg direction="out" type="b"/></method>
<method name="GetAccessibleAtPoint"><arg direction="in" name="x" type="i"/><arg direction="in" name="y" type="i"/><arg direction="in" name="coordType" type="u"/><arg direction="out" type="(so)"/></method>
<method name="GetExtents"><arg direction="in" name="coordType" type="u"/><arg direction="out" type="(iiii)"/></method>
<method name="GetPosition"><arg direction="in" name="coordType" type="u"/><arg direction="out" type="i"/><arg direction="out" type="i"/></method>
<method name="GetSize"><arg direction="out" type="i"/><arg direction="out" type="i"/></method>
<method name="GetLayer"><arg direction="out" type="u"/></method>
<method name="GetMDIZOrder"><arg direction="out" type="n"/></method>
<method name="GrabFocus"><arg direction="out" type="b"/></method>
<method name="GetAlpha"><arg direction="out" type="d"/></method>
</interface>
<interface name="org.a11y.atspi.Action">
<property name="NActions" type="i" access="read"/>
<method name="GetDescription"><arg direction="in" name="index" type="i"/><arg direction="out" type="s"/></method>
<method name="GetName"><arg direction="in" name="index" type="i"/><arg direction="out" type="s"/></method>
<method name="GetLocalizedName"><arg direction="in" name="index" type="i"/><arg direction="out" type="s"/></method>
<method name="GetKeyBinding"><arg direction="in" name="index" type="i"/><arg direction="out" type="s"/></method>
<method name="GetActions"><arg direction="out" type="a(sss)"/></method>
<method name="DoAction"><arg direction="in" name="index" type="i"/><arg direction="out" type="b"/></method>
</interface>
<interface name="org.a11y.atspi.Value">
<property name="MinimumValue" type="d" access="read"/><property name="MaximumValue" type="d" access="read"/><property name="MinimumIncrement" type="d" access="read"/><property name="CurrentValue" type="d" access="readwrite"/><property name="Text" type="s" access="read"/>
</interface>
<interface name="org.a11y.atspi.Cache">
<method name="GetItems"><arg direction="out" type="a((so)(so)(so)iiassusau)"/></method>
</interface>
<interface name="org.a11y.atspi.Application">
<property name="ToolkitName" type="s" access="read"/><property name="Version" type="s" access="read"/><property name="AtspiVersion" type="s" access="read"/><property name="Id" type="i" access="readwrite"/>
<method name="GetLocale"><arg direction="in" name="lctype" type="u"/><arg direction="out" type="s"/></method>
</interface>
</node>)xml";

    static void (*busGet)(int type, void* cancellable, ReadyCallback callback, void* data);
    static void* (*busGetFinish)(void* result, void** error);
    static void (*call)(void* connection, const char* name, const char* path, const char* interface, const char* method, void* parameters, const char* replyType, int flags, int timeout, void* cancellable, ReadyCallback callback, void* data);
    static void* (*callFinish)(void* connection, void* result, void** error);
    static void (*connect)(const char* address, int flags, void* observer, void* cancellable, ReadyCallback callback, void* data);
    static void* (*connectFinish)(void* result, void** error);
    static unsigned int (*registerSubtree)(void* connection, const char* path, const SubtreeTable* table, int flags, void* data, void (*destroy)(void*), void** error);
    static unsigned int (*registerObject)(void* connection, const char* path, void* interface, const InterfaceTable* table, void* data, void (*destroy)(void*), void** error);
    static int (*emitSignal)(void* connection, const char* destination, const char* path, const char* interface, const char* signal, void* parameters, void** error);
    static unsigned int (*subscribe)(void* connection, const char* sender, const char* interface, const char* member, const char* path, const char* argument, int flags, SignalCallback callback, void* data, void (*release)(void*));
    static const char* (*getUniqueName)(void* connection);
    static void (*returnValue)(void* invocation, void* value);
    static void (*returnError)(void* invocation, const char* name, const char* message);
    static void* (*parseNode)(const char* xml, void** error);
    static void* (*lookupInterface)(void* node, const char* name);
    static void* (*refInterface)(void* info);
    static void* (*newVariant)(const char* format, ...);
    static void (*unpack)(void* variant, const char* format, ...);
    static void* (*getChild)(void* variant, std::size_t index);
    static std::size_t (*countChildren)(void* variant);
    static const char* (*getString)(void* variant, std::size_t* length);
    static int (*getBoolean)(void* variant);
    static void* (*getInner)(void* variant);
    static void (*unref)(void* variant);
    static void* (*newBuilder)(const char* type);
    static void (*addToBuilder)(void* builder, const char* format, ...);
    static void* (*endBuilder)(void* builder);
    static void (*unrefBuilder)(void* builder);
    static void* (*allocateZeroed)(std::size_t size);

    static const InterfaceTable kInterfaceTable;
    static const InterfaceTable kCacheTable;
    static const SubtreeTable kSubtreeTable;

    static void* bus;
    static void* node;
    static std::string uniqueName;
    static std::string parentName;
    static std::string parentPath;
    static bool enabled;
    static bool screenReader;
    static bool asked;
    static bool active;
    static int applicationId;
    static std::vector<AccessibilityElement> elements;
    static std::unordered_map<std::uint64_t, std::size_t> indices;
    static std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> children;
    static std::uint64_t focusedElement;

    [[nodiscard]] static bool load(void* library);

    static void sessionReady(void* source, void* result, void* data);
    static void statusRead(void* source, void* result, void* data);
    static void statusChanged(void* connection, const char* sender, const char* path, const char* interface, const char* signal, void* parameters, void* data);
    static void applyStatus(void* entries);
    static void addressRead(void* source, void* result, void* data);
    static void busReady(void* source, void* result, void* data);
    static void embedded(void* source, void* result, void* data);

    static char** enumerate(void* connection, const char* sender, const char* path, void* data);
    static void** introspect(void* connection, const char* sender, const char* path, const char* node, void* data);
    static const void* dispatch(void* connection, const char* sender, const char* path, const char* interface, const char* node, void** out, void* data);
    static void answerCache(void* connection, const char* sender, const char* path, const char* interface, const char* method, void* parameters, void* invocation, void* data);
    static void answer(void* connection, const char* sender, const char* path, const char* interface, const char* method, void* parameters, void* invocation, void* data);
    static void* readProperty(void* connection, const char* sender, const char* path, const char* interface, const char* property, void** error, void* data);
    static int writeProperty(void* connection, const char* sender, const char* path, const char* interface, const char* property, void* value, void** error, void* data);

    [[nodiscard]] static Object findObject(const char* path);
    [[nodiscard]] static const AccessibilityElement* findElement(std::uint64_t id);
    [[nodiscard]] static std::string getPath(const Object& object);
    [[nodiscard]] static void* newReference(const Object& object);
    [[nodiscard]] static Object getParent(const Object& object);
    [[nodiscard]] static std::span<const std::uint64_t> getChildren(const Object& object);
    [[nodiscard]] static std::vector<const char*> getInterfaces(const Object& object);
    [[nodiscard]] static std::string getName(const Object& object);
    [[nodiscard]] static std::string getDescription(const Object& object);
    [[nodiscard]] static std::uint32_t getRole(const Object& object);
    [[nodiscard]] static std::string_view getRoleName(const Object& object);
    [[nodiscard]] static std::array<std::uint32_t, 2> getStates(const Object& object);
    [[nodiscard]] static std::array<std::uint32_t, 2> getStates(const AccessibilityElement& element, bool focused);
    [[nodiscard]] static math::Rect getBounds(const Object& object, std::uint32_t coordinates);
    [[nodiscard]] static Object findAt(const Object& object, math::Vec2 point);
    [[nodiscard]] static std::vector<AccessibilityElement::Action> getActions(const AccessibilityElement& element);
    [[nodiscard]] static bool hasValue(const AccessibilityElement& element) noexcept;
    [[nodiscard]] static double readNumber(std::string_view text) noexcept;
    [[nodiscard]] static std::string toUtf8(std::string_view text);

    static void answerAccessible(const Object& object, const char* method, void* parameters, void* invocation);
    static void answerComponent(const Object& object, const char* method, void* parameters, void* invocation);
    static void answerAction(const Object& object, const char* method, void* parameters, void* invocation);
    static void perform(const Object& object, AccessibilityElement::Action action);

    static void emit(const Object& object, const char* interface, const char* signal, const char* detail, int first, int second, void* data);
    static void announceChildren(const std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>& before);
    static void announceChanges(const std::vector<AccessibilityElement>& before, const std::unordered_map<std::uint64_t, std::size_t>& indicesBefore, std::uint64_t focusedBefore);
};

} // namespace haylen::platform
