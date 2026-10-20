#include "platform/linux/LinuxAccessibility.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>

#include "haylen/core/Utf8.hpp"
#include "haylen/core/Version.hpp"
#include "platform/AccessibilityRelay.hpp"
#include "platform/linux/LinuxDesktop.hpp"
#include "platform/linux/LinuxGlib.hpp"
#include "platform/linux/LinuxSettings.hpp"
#include "sokol_app.h"

namespace haylen::platform {

void (*LinuxAccessibility::busGet)(int, void*, ReadyCallback, void*) = nullptr;
void* (*LinuxAccessibility::busGetFinish)(void*, void**) = nullptr;
void (*LinuxAccessibility::call)(void*, const char*, const char*, const char*, const char*, void*, const char*, int, int, void*, ReadyCallback, void*) = nullptr;
void* (*LinuxAccessibility::callFinish)(void*, void*, void**) = nullptr;
void (*LinuxAccessibility::connect)(const char*, int, void*, void*, ReadyCallback, void*) = nullptr;
void* (*LinuxAccessibility::connectFinish)(void*, void**) = nullptr;
unsigned int (*LinuxAccessibility::registerSubtree)(void*, const char*, const SubtreeTable*, int, void*, void (*)(void*), void**) = nullptr;
unsigned int (*LinuxAccessibility::registerObject)(void*, const char*, void*, const InterfaceTable*, void*, void (*)(void*), void**) = nullptr;
int (*LinuxAccessibility::emitSignal)(void*, const char*, const char*, const char*, const char*, void*, void**) = nullptr;
unsigned int (*LinuxAccessibility::subscribe)(void*, const char*, const char*, const char*, const char*, const char*, int, SignalCallback, void*, void (*)(void*)) = nullptr;
const char* (*LinuxAccessibility::getUniqueName)(void*) = nullptr;
void (*LinuxAccessibility::returnValue)(void*, void*) = nullptr;
void (*LinuxAccessibility::returnError)(void*, const char*, const char*) = nullptr;
void* (*LinuxAccessibility::parseNode)(const char*, void**) = nullptr;
void* (*LinuxAccessibility::lookupInterface)(void*, const char*) = nullptr;
void* (*LinuxAccessibility::refInterface)(void*) = nullptr;
void* (*LinuxAccessibility::newVariant)(const char*, ...) = nullptr;
void (*LinuxAccessibility::unpack)(void*, const char*, ...) = nullptr;
void* (*LinuxAccessibility::getChild)(void*, std::size_t) = nullptr;
std::size_t (*LinuxAccessibility::countChildren)(void*) = nullptr;
const char* (*LinuxAccessibility::getString)(void*, std::size_t*) = nullptr;
int (*LinuxAccessibility::getBoolean)(void*) = nullptr;
void* (*LinuxAccessibility::getInner)(void*) = nullptr;
void (*LinuxAccessibility::unref)(void*) = nullptr;
void* (*LinuxAccessibility::newBuilder)(const char*) = nullptr;
void (*LinuxAccessibility::addToBuilder)(void*, const char*, ...) = nullptr;
void* (*LinuxAccessibility::endBuilder)(void*) = nullptr;
void (*LinuxAccessibility::unrefBuilder)(void*) = nullptr;
void* (*LinuxAccessibility::allocateZeroed)(std::size_t) = nullptr;

const LinuxAccessibility::InterfaceTable LinuxAccessibility::kInterfaceTable{.method = answer, .get = readProperty, .set = writeProperty};
const LinuxAccessibility::InterfaceTable LinuxAccessibility::kCacheTable{.method = answerCache};
const LinuxAccessibility::SubtreeTable LinuxAccessibility::kSubtreeTable{.enumerate = enumerate, .introspect = introspect, .dispatch = dispatch};

void* LinuxAccessibility::bus = nullptr;
void* LinuxAccessibility::node = nullptr;
std::string LinuxAccessibility::uniqueName;
std::string LinuxAccessibility::parentName;
std::string LinuxAccessibility::parentPath = kNullPath;
bool LinuxAccessibility::enabled = false;
bool LinuxAccessibility::screenReader = false;
bool LinuxAccessibility::asked = false;
bool LinuxAccessibility::active = false;
int LinuxAccessibility::applicationId = 0;
std::vector<AccessibilityElement> LinuxAccessibility::elements;
std::unordered_map<std::uint64_t, std::size_t> LinuxAccessibility::indices;
std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> LinuxAccessibility::children;
std::uint64_t LinuxAccessibility::focusedElement = 0;

// The session bus names the accessibility bus, where the app serves its objects, and tells whether assistive technologies run.
void LinuxAccessibility::start() {
    void* library = LinuxGlib::open();
    if (library == nullptr || !load(library)) {
        return;
    }
    node = parseNode(kIntrospection, nullptr);
    if (node != nullptr) {
        busGet(kSessionBus, nullptr, sessionReady, nullptr);
    }
}

bool LinuxAccessibility::load(void* library) {
    return LinuxGlib::find(library, busGet, "g_bus_get") && LinuxGlib::find(library, busGetFinish, "g_bus_get_finish") && LinuxGlib::find(library, call, "g_dbus_connection_call") && LinuxGlib::find(library, callFinish, "g_dbus_connection_call_finish") && LinuxGlib::find(library, connect, "g_dbus_connection_new_for_address") && LinuxGlib::find(library, connectFinish, "g_dbus_connection_new_for_address_finish") && LinuxGlib::find(library, registerSubtree, "g_dbus_connection_register_subtree") && LinuxGlib::find(library, registerObject, "g_dbus_connection_register_object") && LinuxGlib::find(library, emitSignal, "g_dbus_connection_emit_signal") && LinuxGlib::find(library, subscribe, "g_dbus_connection_signal_subscribe") && LinuxGlib::find(library, getUniqueName, "g_dbus_connection_get_unique_name") && LinuxGlib::find(library, returnValue, "g_dbus_method_invocation_return_value") && LinuxGlib::find(library, returnError, "g_dbus_method_invocation_return_dbus_error") && LinuxGlib::find(library, parseNode, "g_dbus_node_info_new_for_xml") && LinuxGlib::find(library, lookupInterface, "g_dbus_node_info_lookup_interface") && LinuxGlib::find(library, refInterface, "g_dbus_interface_info_ref") && LinuxGlib::find(library, newVariant, "g_variant_new") && LinuxGlib::find(library, unpack, "g_variant_get") && LinuxGlib::find(library, getChild, "g_variant_get_child_value") && LinuxGlib::find(library, countChildren, "g_variant_n_children") && LinuxGlib::find(library, getString, "g_variant_get_string") && LinuxGlib::find(library, getBoolean, "g_variant_get_boolean") && LinuxGlib::find(library, getInner, "g_variant_get_variant") && LinuxGlib::find(library, unref, "g_variant_unref") && LinuxGlib::find(library, newBuilder, "g_variant_builder_new") && LinuxGlib::find(library, addToBuilder, "g_variant_builder_add") && LinuxGlib::find(library, endBuilder, "g_variant_builder_end") && LinuxGlib::find(library, unrefBuilder, "g_variant_builder_unref") && LinuxGlib::find(library, allocateZeroed, "g_malloc0");
}

bool LinuxAccessibility::isWanted() noexcept {
    return bus != nullptr && (enabled || screenReader || asked);
}

void LinuxAccessibility::sessionReady(void*, void* result, void*) {
    void* session = busGetFinish(result, nullptr);
    if (session == nullptr) {
        return;
    }
    subscribe(session, "org.a11y.Bus", "org.freedesktop.DBus.Properties", "PropertiesChanged", "/org/a11y/bus", "org.a11y.Status", 0, statusChanged, nullptr, nullptr);
    call(session, "org.a11y.Bus", "/org/a11y/bus", "org.freedesktop.DBus.Properties", "GetAll", newVariant("(s)", "org.a11y.Status"), "(a{sv})", 0, kTimeoutMilliseconds, nullptr, statusRead, nullptr);
    call(session, "org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress", nullptr, "(s)", 0, kTimeoutMilliseconds, nullptr, addressRead, nullptr);
}

void LinuxAccessibility::statusRead(void* source, void* result, void*) {
    void* reply = callFinish(source, result, nullptr);
    if (reply == nullptr) {
        return;
    }
    void* entries = getChild(reply, 0);
    applyStatus(entries);
    unref(entries);
    unref(reply);
}

// The bus signals the properties of its status that changed, with their names and values in its second argument.
void LinuxAccessibility::statusChanged(void*, const char*, const char*, const char*, const char*, void* parameters, void*) {
    void* entries = getChild(parameters, 1);
    applyStatus(entries);
    unref(entries);
}

// An assistive technology that runs turns `IsEnabled` on, and a screen reader also `ScreenReaderEnabled`, which the accessibility settings report.
void LinuxAccessibility::applyStatus(void* entries) {
    for (std::size_t index = 0, count = countChildren(entries); index < count; ++index) {
        void* entry = getChild(entries, index);
        void* key = getChild(entry, 0);
        void* boxed = getChild(entry, 1);
        void* value = getInner(boxed);
        const std::string_view name = getString(key, nullptr);
        if (name == "IsEnabled") {
            enabled = getBoolean(value) != 0;
        } else if (name == "ScreenReaderEnabled") {
            screenReader = getBoolean(value) != 0;
            LinuxSettings::setScreenReader(screenReader);
        }
        unref(value);
        unref(boxed);
        unref(key);
        unref(entry);
    }
}

void LinuxAccessibility::addressRead(void* source, void* result, void*) {
    void* reply = callFinish(source, result, nullptr);
    if (reply == nullptr) {
        return;
    }
    void* address = getChild(reply, 0);
    connect(getString(address, nullptr), kConnectionFlags, nullptr, nullptr, busReady, nullptr);
    unref(address);
    unref(reply);
}

// The objects of the app answer under one path, and the registry of the bus lists the app once it embeds its root.
void LinuxAccessibility::busReady(void*, void* result, void*) {
    bus = connectFinish(result, nullptr);
    if (bus == nullptr) {
        return;
    }
    uniqueName = getUniqueName(bus);
    registerSubtree(bus, kPrefix, &kSubtreeTable, kDispatchToUnenumerated, nullptr, nullptr, nullptr);
    registerObject(bus, kCachePath, lookupInterface(node, kCache), &kCacheTable, nullptr, nullptr, nullptr);
    call(bus, "org.a11y.atspi.Registry", kRootPath, "org.a11y.atspi.Socket", "Embed", newVariant("((so))", uniqueName.c_str(), kRootPath), "((so))", 0, kTimeoutMilliseconds, nullptr, embedded, nullptr);
}

// The registry answers with the desktop, the parent of the root of the app.
void LinuxAccessibility::embedded(void* source, void* result, void*) {
    void* reply = callFinish(source, result, nullptr);
    if (reply == nullptr) {
        return;
    }
    void* reference = getChild(reply, 0);
    void* name = getChild(reference, 0);
    void* path = getChild(reference, 1);
    parentName = getString(name, nullptr);
    parentPath = getString(path, nullptr);
    unref(path);
    unref(name);
    unref(reference);
    unref(reply);
}

void LinuxAccessibility::update(std::span<const AccessibilityElement> tree, std::uint64_t focused) {
    std::vector<AccessibilityElement> before = std::move(elements);
    std::unordered_map<std::uint64_t, std::size_t> indicesBefore = std::move(indices);
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> childrenBefore = std::move(children);
    const std::uint64_t focusedBefore = focusedElement;

    elements.assign(tree.begin(), tree.end());
    indices.clear();
    children.clear();
    for (std::size_t index = 0; index < elements.size(); ++index) {
        indices.emplace(elements[index].id, index);
        children[elements[index].parent].push_back(elements[index].id);
    }
    focusedElement = focused;

    if (bus != nullptr) {
        announceChildren(childrenBefore);
        announceChanges(before, indicesBefore, focusedBefore);
    }
}

// The window announces the text with the politeness of a live region, as screen readers that read the announcements of AT-SPI expect.
void LinuxAccessibility::announce(std::string_view text, bool interrupt) {
    if (bus != nullptr) {
        emit({.kind = Object::Kind::Window}, kObjectEvents, "Announcement", "", interrupt ? kAssertive : kPolite, 0, newVariant("s", toUtf8(text).c_str()));
    }
}

void LinuxAccessibility::setActive(bool value) {
    active = value;
    if (bus == nullptr) {
        return;
    }
    const Object window{.kind = Object::Kind::Window};
    emit(window, kObjectEvents, "StateChanged", "active", value ? 1 : 0, 0, newVariant("i", 0));
    emit(window, kWindowEvents, value ? "Activate" : "Deactivate", "", 0, 0, newVariant("s", getName(window).c_str()));
}

// The subtree has no fixed children to list, since its elements come and go with the frames.
char** LinuxAccessibility::enumerate(void*, const char*, const char*, void*) {
    return static_cast<char**>(allocateZeroed(sizeof(char*)));
}

void** LinuxAccessibility::introspect(void*, const char*, const char*, const char* name, void*) {
    const Object object = name == nullptr ? Object{} : findObject((std::string(kPrefix) + "/" + name).c_str());
    if (object.kind == Object::Kind::None) {
        return nullptr;
    }
    const std::vector<const char*> interfaces = getInterfaces(object);
    auto** infos = static_cast<void**>(allocateZeroed((interfaces.size() + 1) * sizeof(void*)));
    for (std::size_t index = 0; index < interfaces.size(); ++index) {
        infos[index] = refInterface(lookupInterface(node, interfaces[index]));
    }
    return infos;
}

const void* LinuxAccessibility::dispatch(void*, const char*, const char*, const char* interface, const char* name, void** out, void*) {
    const Object object = name == nullptr ? Object{} : findObject((std::string(kPrefix) + "/" + name).c_str());
    const std::vector<const char*> interfaces = object.kind == Object::Kind::None ? std::vector<const char*>{} : getInterfaces(object);
    if (std::ranges::none_of(interfaces, [interface](const char* offered) { return std::strcmp(offered, interface) == 0; })) {
        return nullptr;
    }
    *out = nullptr;
    return &kInterfaceTable;
}

// Screen readers ask the cache for every object of an app at once, and an empty cache makes them ask each object, whose elements change with the frames.
void LinuxAccessibility::answerCache(void*, const char*, const char*, const char*, const char*, void*, void* invocation, void*) {
    void* builder = newBuilder("a((so)(so)(so)iiassusau)");
    void* items = endBuilder(builder);
    unrefBuilder(builder);
    returnValue(invocation, newVariant("(@a((so)(so)(so)iiassusau))", items));
}

void LinuxAccessibility::answer(void*, const char*, const char* path, const char* interface, const char* method, void* parameters, void* invocation, void*) {
    const Object object = findObject(path);
    if (object.kind == Object::Kind::None) {
        returnError(invocation, "org.freedesktop.DBus.Error.UnknownObject", "The element is gone.");
        return;
    }
    if (std::strcmp(interface, kAccessible) == 0) {
        answerAccessible(object, method, parameters, invocation);
    } else if (std::strcmp(interface, kComponent) == 0) {
        answerComponent(object, method, parameters, invocation);
    } else if (std::strcmp(interface, kAction) == 0) {
        answerAction(object, method, parameters, invocation);
    } else if (std::strcmp(interface, kApplication) == 0) {
        returnValue(invocation, newVariant("(s)", ""));
    } else {
        returnError(invocation, "org.freedesktop.DBus.Error.UnknownMethod", "The element has no such method.");
    }
}

// A screen reader that asks for the children of the app reads it, so the UI builds the tree from then on.
void LinuxAccessibility::answerAccessible(const Object& object, const char* method, void* parameters, void* invocation) {
    const std::string_view name = method;
    if (name == "GetChildren" || name == "GetChildAtIndex") {
        asked = true;
    }
    if (name == "GetChildAtIndex") {
        int index = 0;
        unpack(parameters, "(i)", &index);
        if (object.kind == Object::Kind::Root) {
            returnValue(invocation, newVariant("(@(so))", index == 0 ? newReference({.kind = Object::Kind::Window}) : newVariant("(so)", "", kNullPath)));
            return;
        }
        const std::span<const std::uint64_t> found = getChildren(object);
        const bool inside = index >= 0 && static_cast<std::size_t>(index) < found.size();
        returnValue(invocation, newVariant("(@(so))", inside ? newReference({.kind = Object::Kind::Element, .id = found[static_cast<std::size_t>(index)]}) : newVariant("(so)", "", kNullPath)));
    } else if (name == "GetChildren") {
        void* builder = newBuilder("a(so)");
        if (object.kind == Object::Kind::Root) {
            addToBuilder(builder, "@(so)", newReference({.kind = Object::Kind::Window}));
        } else {
            for (const std::uint64_t child : getChildren(object)) {
                addToBuilder(builder, "@(so)", newReference({.kind = Object::Kind::Element, .id = child}));
            }
        }
        void* list = endBuilder(builder);
        unrefBuilder(builder);
        returnValue(invocation, newVariant("(@a(so))", list));
    } else if (name == "GetIndexInParent") {
        int index = 0;
        if (object.kind == Object::Kind::Element) {
            const std::span<const std::uint64_t> siblings = getChildren(getParent(object));
            index = static_cast<int>(std::ranges::find(siblings, object.id) - siblings.begin());
        }
        returnValue(invocation, newVariant("(i)", object.kind == Object::Kind::Root ? -1 : index));
    } else if (name == "GetRelationSet") {
        void* builder = newBuilder("a(ua(so))");
        void* relations = endBuilder(builder);
        unrefBuilder(builder);
        returnValue(invocation, newVariant("(@a(ua(so)))", relations));
    } else if (name == "GetRole") {
        returnValue(invocation, newVariant("(u)", getRole(object)));
    } else if (name == "GetRoleName" || name == "GetLocalizedRoleName") {
        returnValue(invocation, newVariant("(s)", std::string(getRoleName(object)).c_str()));
    } else if (name == "GetState") {
        const std::array<std::uint32_t, 2> states = getStates(object);
        void* builder = newBuilder("au");
        addToBuilder(builder, "u", states[0]);
        addToBuilder(builder, "u", states[1]);
        void* list = endBuilder(builder);
        unrefBuilder(builder);
        returnValue(invocation, newVariant("(@au)", list));
    } else if (name == "GetAttributes") {
        void* builder = newBuilder("a{ss}");
        addToBuilder(builder, "{ss}", "toolkit", "Haylen");
        void* attributes = endBuilder(builder);
        unrefBuilder(builder);
        returnValue(invocation, newVariant("(@a{ss})", attributes));
    } else if (name == "GetApplication") {
        returnValue(invocation, newVariant("(@(so))", newReference({.kind = Object::Kind::Root})));
    } else if (name == "GetInterfaces") {
        void* builder = newBuilder("as");
        for (const char* offered : getInterfaces(object)) {
            addToBuilder(builder, "s", offered);
        }
        void* list = endBuilder(builder);
        unrefBuilder(builder);
        returnValue(invocation, newVariant("(@as)", list));
    } else {
        returnError(invocation, "org.freedesktop.DBus.Error.UnknownMethod", "The element has no such method.");
    }
}

void LinuxAccessibility::answerComponent(const Object& object, const char* method, void* parameters, void* invocation) {
    const std::string_view name = method;
    if (name == "Contains" || name == "GetAccessibleAtPoint") {
        int x = 0;
        int y = 0;
        std::uint32_t coordinates = 0;
        unpack(parameters, "(iiu)", &x, &y, &coordinates);
        const math::Rect area = getBounds(object, coordinates);
        const bool inside = area.contains(math::Vec2{static_cast<float>(x), static_cast<float>(y)});
        if (name == "Contains") {
            returnValue(invocation, newVariant("(b)", inside ? 1 : 0));
            return;
        }
        // The point moves into the coordinates of the window, where the elements lie.
        const math::Vec2 shift = getBounds(object, 1).getMin() - area.getMin();
        returnValue(invocation, newVariant("(@(so))", inside ? newReference(findAt(object, math::Vec2{static_cast<float>(x), static_cast<float>(y)} + shift)) : newVariant("(so)", "", kNullPath)));
    } else if (name == "GetExtents" || name == "GetPosition") {
        std::uint32_t coordinates = 0;
        unpack(parameters, "(u)", &coordinates);
        const math::Rect area = getBounds(object, coordinates);
        const auto x = static_cast<int>(std::lround(area.x));
        const auto y = static_cast<int>(std::lround(area.y));
        if (name == "GetPosition") {
            returnValue(invocation, newVariant("(ii)", x, y));
            return;
        }
        returnValue(invocation, newVariant("((iiii))", x, y, static_cast<int>(std::lround(area.width)), static_cast<int>(std::lround(area.height))));
    } else if (name == "GetSize") {
        const math::Rect area = getBounds(object, 1);
        returnValue(invocation, newVariant("(ii)", static_cast<int>(std::lround(area.width)), static_cast<int>(std::lround(area.height))));
    } else if (name == "GetLayer") {
        returnValue(invocation, newVariant("(u)", object.kind == Object::Kind::Element ? kLayerWidget : kLayerWindow));
    } else if (name == "GetMDIZOrder") {
        returnValue(invocation, newVariant("(n)", 0));
    } else if (name == "GrabFocus") {
        const AccessibilityElement* element = object.kind == Object::Kind::Element ? findElement(object.id) : nullptr;
        const bool focusable = element != nullptr && element->can(AccessibilityElement::Focus);
        if (focusable) {
            perform(object, AccessibilityElement::Focus);
        }
        returnValue(invocation, newVariant("(b)", focusable ? 1 : 0));
    } else if (name == "GetAlpha") {
        returnValue(invocation, newVariant("(d)", 1.0));
    } else {
        returnError(invocation, "org.freedesktop.DBus.Error.UnknownMethod", "The element has no such method.");
    }
}

void LinuxAccessibility::answerAction(const Object& object, const char* method, void* parameters, void* invocation) {
    const std::string_view name = method;
    const AccessibilityElement* element = findElement(object.id);
    const std::vector<AccessibilityElement::Action> actions = element != nullptr ? getActions(*element) : std::vector<AccessibilityElement::Action>{};
    const auto nameOf = [](AccessibilityElement::Action action) { return std::ranges::find(kActions, action, &std::pair<AccessibilityElement::Action, std::string_view>::first)->second.data(); };
    if (name == "GetActions") {
        void* builder = newBuilder("a(sss)");
        for (const AccessibilityElement::Action action : actions) {
            addToBuilder(builder, "(sss)", nameOf(action), "", "");
        }
        void* list = endBuilder(builder);
        unrefBuilder(builder);
        returnValue(invocation, newVariant("(@a(sss))", list));
        return;
    }
    int index = 0;
    unpack(parameters, "(i)", &index);
    const bool valid = index >= 0 && static_cast<std::size_t>(index) < actions.size();
    if (name == "DoAction") {
        if (valid) {
            perform(object, actions[static_cast<std::size_t>(index)]);
        }
        returnValue(invocation, newVariant("(b)", valid ? 1 : 0));
    } else if (name == "GetName" || name == "GetLocalizedName") {
        returnValue(invocation, newVariant("(s)", valid ? nameOf(actions[static_cast<std::size_t>(index)]) : ""));
    } else if (name == "GetDescription" || name == "GetKeyBinding") {
        returnValue(invocation, newVariant("(s)", ""));
    } else {
        returnError(invocation, "org.freedesktop.DBus.Error.UnknownMethod", "The element has no such method.");
    }
}

void* LinuxAccessibility::readProperty(void*, const char*, const char* path, const char* interface, const char* property, void**, void*) {
    const Object object = findObject(path);
    const std::string_view name = property;
    if (std::strcmp(interface, kAccessible) == 0) {
        if (name == "Name") {
            return newVariant("s", getName(object).c_str());
        }
        if (name == "Description") {
            return newVariant("s", getDescription(object).c_str());
        }
        if (name == "Parent") {
            if (object.kind == Object::Kind::Root) {
                return newVariant("(so)", parentName.c_str(), parentPath.c_str());
            }
            return newReference(getParent(object));
        }
        if (name == "ChildCount") {
            return newVariant("i", object.kind == Object::Kind::Root ? 1 : static_cast<int>(getChildren(object).size()));
        }
        if (name == "AccessibleId") {
            return newVariant("s", object.kind == Object::Kind::Element ? std::to_string(object.id).c_str() : "");
        }
        if (name == "HelpText") {
            const AccessibilityElement* element = findElement(object.id);
            return newVariant("s", object.kind == Object::Kind::Element && element != nullptr ? toUtf8(element->hint).c_str() : "");
        }
        return newVariant("s", "");
    }
    if (std::strcmp(interface, kAction) == 0) {
        const AccessibilityElement* element = findElement(object.id);
        return newVariant("i", element != nullptr ? static_cast<int>(getActions(*element).size()) : 0);
    }
    if (std::strcmp(interface, kValue) == 0) {
        // The engine knows the number of a value and not its range, so the range is the value itself and the text says the rest.
        const AccessibilityElement* element = findElement(object.id);
        const std::string text = element != nullptr ? toUtf8(element->value) : std::string();
        if (name == "Text") {
            return newVariant("s", text.c_str());
        }
        return newVariant("d", name == "MinimumIncrement" ? 0.0 : readNumber(text));
    }
    if (std::strcmp(interface, kApplication) == 0) {
        if (name == "Id") {
            return newVariant("i", applicationId);
        }
        if (name == "ToolkitName") {
            return newVariant("s", "Haylen");
        }
        if (name == "Version") {
            return newVariant("s", std::string(core::Version::kString).c_str());
        }
        return newVariant("s", "2.1");
    }
    return nullptr;
}

// The registry numbers the app, and a screen reader that sets the value of a slider moves it a step toward the new value.
int LinuxAccessibility::writeProperty(void*, const char*, const char* path, const char* interface, const char* property, void* value, void**, void*) {
    const Object object = findObject(path);
    if (std::strcmp(interface, kApplication) == 0 && std::strcmp(property, "Id") == 0) {
        unpack(value, "i", &applicationId);
        return 1;
    }
    const AccessibilityElement* element = findElement(object.id);
    if (std::strcmp(interface, kValue) == 0 && std::strcmp(property, "CurrentValue") == 0 && element != nullptr) {
        double target = 0.0;
        unpack(value, "d", &target);
        const double current = readNumber(element->value);
        if (target > current && element->can(AccessibilityElement::Increment)) {
            perform(object, AccessibilityElement::Increment);
        } else if (target < current && element->can(AccessibilityElement::Decrement)) {
            perform(object, AccessibilityElement::Decrement);
        }
        return 1;
    }
    return 0;
}

void LinuxAccessibility::perform(const Object& object, AccessibilityElement::Action action) {
    if (object.kind == Object::Kind::Element) {
        AccessibilityRelay::perform({.element = object.id, .action = action});
    }
}

LinuxAccessibility::Object LinuxAccessibility::findObject(const char* path) {
    const std::string_view text = path;
    if (text == kRootPath) {
        return {.kind = Object::Kind::Root};
    }
    if (text == kWindowPath) {
        return {.kind = Object::Kind::Window};
    }
    const std::string_view prefix = kPrefix;
    if (!text.starts_with(prefix) || text.size() <= prefix.size() + 1 || text[prefix.size()] != '/') {
        return {};
    }
    std::uint64_t id = 0;
    const std::string_view digits = text.substr(prefix.size() + 1);
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), id);
    if (error != std::errc{} || end != digits.data() + digits.size() || !indices.contains(id)) {
        return {};
    }
    return {.kind = Object::Kind::Element, .id = id};
}

const AccessibilityElement* LinuxAccessibility::findElement(std::uint64_t id) {
    const auto found = indices.find(id);
    return found != indices.end() ? &elements[found->second] : nullptr;
}

std::string LinuxAccessibility::getPath(const Object& object) {
    switch (object.kind) {
    case Object::Kind::Root:
        return kRootPath;
    case Object::Kind::Window:
        return kWindowPath;
    case Object::Kind::Element:
        return std::string(kPrefix) + "/" + std::to_string(object.id);
    case Object::Kind::None:
        break;
    }
    return kNullPath;
}

void* LinuxAccessibility::newReference(const Object& object) {
    return newVariant("(so)", object.kind == Object::Kind::None ? "" : uniqueName.c_str(), getPath(object).c_str());
}

LinuxAccessibility::Object LinuxAccessibility::getParent(const Object& object) {
    if (object.kind == Object::Kind::Window) {
        return {.kind = Object::Kind::Root};
    }
    const AccessibilityElement* element = object.kind == Object::Kind::Element ? findElement(object.id) : nullptr;
    if (element == nullptr) {
        return {};
    }
    if (element->parent == 0 || !indices.contains(element->parent)) {
        return {.kind = Object::Kind::Window};
    }
    return {.kind = Object::Kind::Element, .id = element->parent};
}

std::span<const std::uint64_t> LinuxAccessibility::getChildren(const Object& object) {
    if (object.kind != Object::Kind::Window && object.kind != Object::Kind::Element) {
        return {};
    }
    const auto found = children.find(object.kind == Object::Kind::Window ? 0 : object.id);
    return found != children.end() ? std::span<const std::uint64_t>(found->second) : std::span<const std::uint64_t>();
}

std::vector<const char*> LinuxAccessibility::getInterfaces(const Object& object) {
    switch (object.kind) {
    case Object::Kind::Root:
        return {kAccessible, kApplication};
    case Object::Kind::Window:
        return {kAccessible, kComponent};
    case Object::Kind::Element:
        break;
    case Object::Kind::None:
        return {};
    }
    std::vector<const char*> interfaces{kAccessible, kComponent};
    const AccessibilityElement& element = *findElement(object.id);
    if (!getActions(element).empty()) {
        interfaces.push_back(kAction);
    }
    if (hasValue(element)) {
        interfaces.push_back(kValue);
    }
    return interfaces;
}

std::string LinuxAccessibility::getName(const Object& object) {
    if (object.kind == Object::Kind::Root || object.kind == Object::Kind::Window) {
        return toUtf8(LinuxDesktop::getTitle());
    }
    const AccessibilityElement* element = findElement(object.id);
    return element != nullptr ? toUtf8(element->label) : std::string();
}

// The value of an element that serves no value interface, such as the text of a field, comes before its hint.
std::string LinuxAccessibility::getDescription(const Object& object) {
    const AccessibilityElement* element = object.kind == Object::Kind::Element ? findElement(object.id) : nullptr;
    if (element == nullptr) {
        return {};
    }
    if (hasValue(*element) || element->value.empty()) {
        return toUtf8(element->hint);
    }
    return toUtf8(element->hint.empty() ? element->value : element->value + ". " + element->hint);
}

std::uint32_t LinuxAccessibility::getRole(const Object& object) {
    if (object.kind == Object::Kind::Root) {
        return kRoleApplication;
    }
    const AccessibilityElement* element = object.kind == Object::Kind::Element ? findElement(object.id) : nullptr;
    return element != nullptr ? kRoles[static_cast<std::size_t>(element->role)] : kRoleFrame;
}

std::string_view LinuxAccessibility::getRoleName(const Object& object) {
    if (object.kind == Object::Kind::Root) {
        return "application";
    }
    const AccessibilityElement* element = object.kind == Object::Kind::Element ? findElement(object.id) : nullptr;
    return element != nullptr ? kRoleNames[static_cast<std::size_t>(element->role)] : "frame";
}

std::array<std::uint32_t, 2> LinuxAccessibility::getStates(const Object& object) {
    if (object.kind == Object::Kind::Element) {
        const AccessibilityElement* element = findElement(object.id);
        return element != nullptr ? getStates(*element, object.id == focusedElement) : std::array<std::uint32_t, 2>{};
    }
    std::array<std::uint32_t, 2> states{};
    if (object.kind == Object::Kind::Window) {
        for (const std::uint32_t state : {kStateEnabled, kStateSensitive, kStateShowing, kStateVisible}) {
            states[state / 32] |= 1U << (state % 32);
        }
        if (active) {
            states[kStateActive / 32] |= 1U << (kStateActive % 32);
        }
    }
    return states;
}

std::array<std::uint32_t, 2> LinuxAccessibility::getStates(const AccessibilityElement& element, bool focused) {
    using Role = AccessibilityElement::Role;
    std::array<std::uint32_t, 2> states{};
    // clang-format off
    const auto set = [&states](std::uint32_t state, bool on) {
        if (on) {
            states[state / 32] |= 1U << (state % 32);
        }
    };
    // clang-format on
    const bool disabled = element.has(AccessibilityElement::Disabled);
    set(kStateEnabled, !disabled);
    set(kStateSensitive, !disabled);
    set(kStateShowing, true);
    set(kStateVisible, true);
    set(kStateFocusable, element.can(AccessibilityElement::Focus));
    set(kStateFocused, focused);
    set(kStateCheckable, element.role == Role::CheckBox || element.role == Role::Switch || element.role == Role::RadioButton);
    set(kStateChecked, element.has(AccessibilityElement::Checked));
    set(kStateSelectable, element.role == Role::Tab || element.role == Role::RadioButton || element.has(AccessibilityElement::Selected));
    set(kStateSelected, element.has(AccessibilityElement::Selected));
    set(kStateExpandable, element.role == Role::ComboBox || element.has(AccessibilityElement::Expanded));
    set(kStateExpanded, element.has(AccessibilityElement::Expanded));
    set(kStateBusy, element.has(AccessibilityElement::Busy));
    set(kStateEditable, element.role == Role::TextField);
    set(kStateSingleLine, element.role == Role::TextField);
    return states;
}

// The screen takes the corner of the window on it, and a parent is the element around, or the window for the elements at the top.
math::Rect LinuxAccessibility::getBounds(const Object& object, std::uint32_t coordinates) {
    const AccessibilityElement* element = object.kind == Object::Kind::Element ? findElement(object.id) : nullptr;
    math::Rect area = element != nullptr ? element->bounds : math::Rect{0.0F, 0.0F, sapp_widthf(), sapp_heightf()};
    if (coordinates == 0) {
        const math::Vec2 origin = LinuxDesktop::getOrigin();
        area.x += origin.x;
        area.y += origin.y;
    } else if (coordinates == 2 && element != nullptr) {
        const AccessibilityElement* parent = findElement(element->parent);
        if (parent != nullptr) {
            area.x -= parent->bounds.x;
            area.y -= parent->bounds.y;
        }
    }
    return area;
}

// The deepest element under a point in the coordinates of the window, where later siblings lie above earlier ones.
LinuxAccessibility::Object LinuxAccessibility::findAt(const Object& object, math::Vec2 point) {
    if (object.kind == Object::Kind::Root) {
        return findAt({.kind = Object::Kind::Window}, point);
    }
    const std::span<const std::uint64_t> found = getChildren(object);
    for (auto child = found.rbegin(); child != found.rend(); ++child) {
        if (findElement(*child)->bounds.contains(point)) {
            return findAt({.kind = Object::Kind::Element, .id = *child}, point);
        }
    }
    return object;
}

std::vector<AccessibilityElement::Action> LinuxAccessibility::getActions(const AccessibilityElement& element) {
    std::vector<AccessibilityElement::Action> actions;
    for (const auto& [action, name] : kActions) {
        if (element.can(action)) {
            actions.push_back(action);
        }
    }
    return actions;
}

bool LinuxAccessibility::hasValue(const AccessibilityElement& element) noexcept {
    return element.role == AccessibilityElement::Role::Slider || element.role == AccessibilityElement::Role::ProgressBar;
}

double LinuxAccessibility::readNumber(std::string_view text) noexcept {
    double number = 0.0;
    std::from_chars(text.data(), text.data() + text.size(), number);
    return number;
}

// GVariant takes only valid UTF-8, so the bytes of a text that is no UTF-8 become replacement characters.
std::string LinuxAccessibility::toUtf8(std::string_view text) {
    if (core::Utf8::isValid(text)) {
        return std::string(text);
    }
    std::string valid;
    for (std::size_t offset = 0; offset < text.size();) {
        core::Utf8::append(valid, core::Utf8::decode(text, offset));
    }
    return valid;
}

void LinuxAccessibility::emit(const Object& object, const char* interface, const char* signal, const char* detail, int first, int second, void* data) {
    void* builder = newBuilder("a{sv}");
    void* properties = endBuilder(builder);
    unrefBuilder(builder);
    emitSignal(bus, nullptr, getPath(object).c_str(), interface, signal, newVariant("(siiv@a{sv})", detail, first, second, data, properties), nullptr);
}

// Each list of children that changed tells what left it and what joined it, with the place of each.
void LinuxAccessibility::announceChildren(const std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>& before) {
    const auto parentOf = [](std::uint64_t id) { return id == 0 ? Object{.kind = Object::Kind::Window} : Object{.kind = Object::Kind::Element, .id = id}; };
    const std::vector<std::uint64_t> none;
    for (const auto& [parent, previous] : before) {
        const auto found = children.find(parent);
        const std::vector<std::uint64_t>& current = found != children.end() ? found->second : none;
        if (current == previous || (parent != 0 && !indices.contains(parent))) {
            continue;
        }
        for (std::size_t index = 0; index < previous.size(); ++index) {
            if (std::ranges::find(current, previous[index]) == current.end()) {
                emit(parentOf(parent), kObjectEvents, "ChildrenChanged", "remove", static_cast<int>(index), 0, newVariant("(so)", uniqueName.c_str(), getPath({.kind = Object::Kind::Element, .id = previous[index]}).c_str()));
            }
        }
    }
    for (const auto& [parent, current] : children) {
        const auto found = before.find(parent);
        const std::vector<std::uint64_t>& previous = found != before.end() ? found->second : none;
        if (current == previous || (parent != 0 && !indices.contains(parent))) {
            continue;
        }
        for (std::size_t index = 0; index < current.size(); ++index) {
            if (std::ranges::find(previous, current[index]) == previous.end()) {
                emit(parentOf(parent), kObjectEvents, "ChildrenChanged", "add", static_cast<int>(index), 0, newReference({.kind = Object::Kind::Element, .id = current[index]}));
            }
        }
    }
}

// The elements that stay tell what changed in their names, descriptions, values and states, and the focus tells where it went.
void LinuxAccessibility::announceChanges(const std::vector<AccessibilityElement>& before, const std::unordered_map<std::uint64_t, std::size_t>& indicesBefore, std::uint64_t focusedBefore) {
    for (const AccessibilityElement& element : elements) {
        const auto found = indicesBefore.find(element.id);
        if (found == indicesBefore.end()) {
            continue;
        }
        const AccessibilityElement& previous = before[found->second];
        const Object object{.kind = Object::Kind::Element, .id = element.id};
        if (element.label != previous.label) {
            emit(object, kObjectEvents, "PropertyChange", "accessible-name", 0, 0, newVariant("s", toUtf8(element.label).c_str()));
        }
        if (hasValue(element) && element.value != previous.value) {
            emit(object, kObjectEvents, "PropertyChange", "accessible-value", 0, 0, newVariant("d", readNumber(element.value)));
        }
        if (element.hint != previous.hint || (!hasValue(element) && element.value != previous.value)) {
            emit(object, kObjectEvents, "PropertyChange", "accessible-description", 0, 0, newVariant("s", getDescription(object).c_str()));
        }
        const std::array<std::uint32_t, 2> states = getStates(element, false);
        const std::array<std::uint32_t, 2> statesBefore = getStates(previous, false);
        for (const auto& [state, name] : kAnnouncedStates) {
            const bool on = (states[state / 32] & (1U << (state % 32))) != 0;
            if (on != ((statesBefore[state / 32] & (1U << (state % 32))) != 0)) {
                emit(object, kObjectEvents, "StateChanged", name, on ? 1 : 0, 0, newVariant("i", 0));
            }
        }
    }

    if (focusedElement == focusedBefore) {
        return;
    }
    if (indices.contains(focusedBefore)) {
        emit({.kind = Object::Kind::Element, .id = focusedBefore}, kObjectEvents, "StateChanged", "focused", 0, 0, newVariant("i", 0));
    }
    if (indices.contains(focusedElement)) {
        const Object object{.kind = Object::Kind::Element, .id = focusedElement};
        emit(object, kObjectEvents, "StateChanged", "focused", 1, 0, newVariant("i", 0));
        emit(object, kFocusEvents, "Focus", "", 0, 0, newVariant("i", 0));
    }
}

} // namespace haylen::platform
