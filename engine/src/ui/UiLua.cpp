#include "ui/UiLua.hpp"

#include <array>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/EventsLua.hpp"
#include "graphics/FontLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Collection.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Scaling.hpp"
#include "lua/Owners.hpp"
#include "lua/ScriptedScene.hpp"
#include "lua/StackScope.hpp"
#include "ui/CollectionLua.hpp"
#include "ui/MountLink.hpp"
#include "ui/TransformLua.hpp"

namespace haylen::lua {

template <> struct Type<ui::Gui> {
    static constexpr const char* name = "haylen.Gui";
    using Storage = std::shared_ptr<ui::Gui>;
};

template <> struct EnumNames<ui::FocusNavigator::Owner> {
    static constexpr std::array<std::pair<std::string_view, ui::FocusNavigator::Owner>, 3> kOwners{{{"none", ui::FocusNavigator::Owner::None}, {"control", ui::FocusNavigator::Owner::Control}, {"playArea", ui::FocusNavigator::Owner::PlayArea}}};

    static std::optional<ui::FocusNavigator::Owner> fromName(std::string_view name) {
        for (const auto& [candidate, owner] : kOwners) {
            if (candidate == name) {
                return owner;
            }
        }
        return std::nullopt;
    }

    static std::string_view name(ui::FocusNavigator::Owner value) {
        for (const auto& [candidate, owner] : kOwners) {
            if (owner == value) {
                return candidate;
            }
        }
        return kOwners.front().first;
    }
};

} // namespace haylen::lua

namespace haylen::ui {

plugins::UiPlugin& UiLua::getPlugin(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<plugins::UiPlugin>();
}

void UiLua::pushRoot(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kHandlersKey);
}

// A handler is a function stored under a key such as `onClick`, and it answers the event named `click`.
std::optional<std::string> UiLua::handlerEvent(lua_State* L, int key, int value) {
    if (lua_type(L, key) != LUA_TSTRING || !lua_isfunction(L, value)) {
        return std::nullopt;
    }
    const std::string_view name = lua::Stack::read<std::string_view>(L, key);
    if (name.size() < 3 || !name.starts_with("on") || std::isupper(static_cast<unsigned char>(name[2])) == 0) {
        return std::nullopt;
    }
    std::string event(name.substr(2));
    event[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(event[0])));
    return event;
}

// Stores the function at the value index as `handlers[id][event]` in the handlers table at the given index.
void UiLua::storeHandler(lua_State* L, int handlers, const std::string& id, const std::string& event, int value) {
    if (lua_getfield(L, handlers, id.c_str()) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, handlers, id.c_str());
    }
    lua_pushvalue(L, value);
    lua_setfield(L, -2, event.c_str());
    lua_pop(L, 1);
}

std::string UiLua::nextGeneratedId(lua_State* L) {
    pushRoot(L);
    lua_getfield(L, -1, "generated");
    const lua_Integer next = lua_tointeger(L, -1) + 1;
    lua_pop(L, 1);
    lua_pushinteger(L, next);
    lua_setfield(L, -2, "generated");
    lua_pop(L, 1);
    return "#" + std::to_string(next);
}

// Converts a node table into the JSON the GUI reads, pulling its handlers into the handlers table. Children come from a `children` list or from the array part of the node, which lets trees read like `ui.column{ui.label{...}}`. The limits of a GUI apply while converting, so a table that holds itself or shares its children many times over raises an error instead of growing without end.
core::Json UiLua::convertNode(lua_State* L, int index, int handlers, std::size_t depth, std::size_t& count) {
    luaL_checktype(L, index, LUA_TTABLE);
    if (depth >= Gui::kMaxDepth || ++count > Gui::kMaxNodes) {
        luaL_error(L, "A GUI is limited to %d levels and %d nodes.", static_cast<int>(Gui::kMaxDepth), static_cast<int>(Gui::kMaxNodes));
    }
    luaL_checkstack(L, LUA_MINSTACK, "the UI tree is nested too deeply");
    const int node = lua_absindex(L, index);
    const int base = lua_gettop(L);
    core::Json json = core::Json::object();
    lua_getfield(L, node, "kind");
    const bool collection = lua_type(L, -1) == LUA_TSTRING && lua::Stack::read<std::string_view>(L, -1) == "collection";
    lua_pop(L, 1);

    lua_newtable(L);
    const int collected = lua_gettop(L);
    bool hasHandlers = false;
    lua_pushnil(L);
    while (lua_next(L, node) != 0) {
        const int key = lua_gettop(L) - 1;
        const int value = lua_gettop(L);
        if (lua_type(L, key) == LUA_TNUMBER) {
            lua_pop(L, 1);
            continue;
        }
        if (lua_type(L, key) != LUA_TSTRING) {
            luaL_error(L, "UI node keys must be strings.");
        }
        if (const std::optional<std::string> event = handlerEvent(L, key, value)) {
            if (handlers == 0) {
                luaL_error(L, "A collection template takes no handlers. Handle the events of its parts on the collection, where they arrive with \"part\" and \"cell\".");
            }
            lua_pushvalue(L, value);
            lua_setfield(L, collected, event->c_str());
            hasHandlers = true;
        } else if (const std::string name = lua::Stack::read<std::string>(L, key); name != "children") {
            json[name] = collection && name == "types" ? convertTypes(L, value, depth, count) : lua::JsonConverter::read(L, value);
        }
        lua_pop(L, 1);
    }

    if (json.contains("id") && json["id"].is_string() && json["id"].get<std::string>().starts_with('#')) {
        luaL_error(L, "UI ids starting with \"#\" are reserved for nodes the engine names.");
    }
    if (hasHandlers) {
        if (!json.contains("id")) {
            json["id"] = nextGeneratedId(L);
        }
        if (!json["id"].is_string()) {
            luaL_error(L, "A UI node with handlers needs a string id.");
        }
        const std::string id = json["id"].get<std::string>();
        lua_pushnil(L);
        while (lua_next(L, collected) != 0) {
            storeHandler(L, handlers, id, lua::Stack::read<std::string>(L, -2), lua_gettop(L));
            lua_pop(L, 1);
        }
    }
    lua_settop(L, base);

    const auto arrayLength = static_cast<lua_Integer>(lua_rawlen(L, node));
    const bool listed = lua_getfield(L, node, "children") != LUA_TNIL;
    if (listed && arrayLength > 0) {
        luaL_error(L, "A UI node takes children either in its \"children\" list or in its array part, not both.");
    }
    if (listed || arrayLength > 0) {
        const int children = listed ? lua_gettop(L) : node;
        luaL_checktype(L, children, LUA_TTABLE);
        core::Json list = core::Json::array();
        const auto length = static_cast<lua_Integer>(lua_rawlen(L, children));
        for (lua_Integer child = 1; child <= length; ++child) {
            lua_rawgeti(L, children, child);
            list.push_back(convertNode(L, -1, handlers, depth + 1, count));
            lua_pop(L, 1);
        }
        json["children"] = std::move(list);
    }
    lua_settop(L, base);
    return json;
}

// Converts the types of a collection, whose templates are node tables without handlers. Values of any other shape go to the collection as they are, which reports what is wrong with them.
core::Json UiLua::convertTypes(lua_State* L, int index, std::size_t depth, std::size_t& count) {
    if (lua_type(L, index) != LUA_TTABLE) {
        return lua::JsonConverter::read(L, index);
    }
    const int types = lua_absindex(L, index);
    core::Json json = core::Json::object();
    lua_pushnil(L);
    while (lua_next(L, types) != 0) {
        const std::string name = lua::Stack::read<std::string>(L, -2);
        if (lua_type(L, -1) != LUA_TTABLE) {
            json[name] = lua::JsonConverter::read(L, -1);
            lua_pop(L, 1);
            continue;
        }
        const int definition = lua_gettop(L);
        core::Json converted = core::Json::object();
        lua_pushnil(L);
        while (lua_next(L, definition) != 0) {
            const std::string key = lua::Stack::read<std::string>(L, -2);
            converted[key] = key == "template" && lua_istable(L, -1) ? convertNode(L, -1, 0, depth + 1, count) : lua::JsonConverter::read(L, -1);
            lua_pop(L, 1);
        }
        json[name] = std::move(converted);
        lua_pop(L, 1);
    }
    return json;
}

// Converts a properties table for `set`, collecting its handlers by event name into the table at the collected index. Every other key must be a property, and the types of a collection convert their templates.
core::Json UiLua::convertProperties(lua_State* L, int index, int collected, bool collection) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    core::Json json = core::Json::object();
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        const int key = lua_gettop(L) - 1;
        const int value = lua_gettop(L);
        if (const std::optional<std::string> event = handlerEvent(L, key, value)) {
            lua_pushvalue(L, value);
            lua_setfield(L, collected, event->c_str());
        } else {
            luaL_argcheck(L, lua_type(L, key) == LUA_TSTRING, index, "property names must be strings");
            const std::string name = lua::Stack::read<std::string>(L, key);
            std::size_t count = 0;
            json[name] = collection && name == "types" ? convertTypes(L, value, 1, count) : lua::JsonConverter::read(L, value);
        }
        lua_pop(L, 1);
    }
    return json;
}

void UiLua::collectIds(const core::Json& node, std::vector<std::string>& ids) {
    if (const auto id = node.find("id"); id != node.end() && id->is_string()) {
        ids.push_back(id->get<std::string>());
    }
    if (const auto children = node.find("children"); children != node.end()) {
        for (const core::Json& child : *children) {
            collectIds(child, ids);
        }
    }
}

// Pushes the handlers table of a mounted GUI, or nothing and `false` when it is not mounted.
bool UiLua::pushHandlers(lua_State* L, const Gui& gui) {
    pushRoot(L);
    lua_rawgetp(L, -1, &gui);
    lua_remove(L, -2);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    lua_getfield(L, -1, "handlers");
    lua_remove(L, -2);
    return true;
}

// Removes handlers of ids the GUI no longer has.
void UiLua::pruneHandlers(lua_State* L, int handlers, const Gui& gui) {
    std::vector<std::string> stale;
    lua_pushnil(L);
    while (lua_next(L, handlers) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && gui.find(lua::Stack::read<std::string_view>(L, -2)) == nullptr) {
            stale.push_back(lua::Stack::read<std::string>(L, -2));
        }
        lua_pop(L, 1);
    }
    for (const std::string& id : stale) {
        lua_pushnil(L);
        lua_setfield(L, handlers, id.c_str());
    }
}

// Makes the nodes of a table of handler tables by node id report the events every kind reports that their handlers answer, such as `press`.
void UiLua::listenToHandlers(lua_State* L, Gui& gui, int handlers) {
    lua_pushnil(L);
    while (lua_next(L, handlers) != 0) {
        listenToEvents(L, gui, lua::Stack::read<std::string_view>(L, -2), lua_gettop(L));
        lua_pop(L, 1);
    }
}

void UiLua::listenToEvents(lua_State* L, Gui& gui, std::string_view id, int events) {
    lua_pushnil(L);
    while (lua_next(L, events) != 0) {
        if (const std::optional<Component::Notice> notice = Component::noticeFromName(lua::Stack::read<std::string_view>(L, -2))) {
            gui.listen(id, *notice, true);
        }
        lua_pop(L, 1);
    }
}

// Pushes the table listeners and handlers receive: the event values plus `id`, `name` and `gui`, which is `nil` for GUIs mounted from C++.
void UiLua::pushEventTable(lua_State* L, const Gui& gui, const Event& event) {
    lua::JsonConverter::push(L, event.value.is_object() ? event.value : core::Json::object());
    lua::Stack::push(L, event.id);
    lua_setfield(L, -2, "id");
    lua::Stack::push(L, event.name);
    lua_setfield(L, -2, "name");
    pushRoot(L);
    if (lua_rawgetp(L, -1, &gui) == LUA_TTABLE) {
        lua_getfield(L, -1, "gui");
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -4, "gui");
    lua_pop(L, 2);
}

Gui& UiLua::checkGui(lua_State* L) {
    return lua::Userdata::check<Gui>(L, 1);
}

// Mounts a tree with `mount(tree[, {placement = 'safe' or 'screen', layer = 0, owner = scene}])` and returns the GUI. A GUI with an owner is unmounted when the owner is released, such as a scene when it unloads.
int UiLua::mount(lua_State* L) {
    Placement placement = Placement::Safe;
    int layer = 0;
    int owner = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kMountFields});
        std::string name = "safe";
        lua::Table::readField(L, 2, "placement", name);
        if (name != "safe" && name != "screen") {
            return luaL_error(L, "The \"placement\" option must be \"safe\" or \"screen\".");
        }
        placement = name == "safe" ? Placement::Safe : Placement::Screen;
        lua::Table::readField(L, 2, "layer", layer);
        if (lua_getfield(L, 2, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }

    lua_newtable(L);
    const int handlers = lua_gettop(L);
    std::size_t count = 0;
    const core::Json tree = convertNode(L, 1, handlers, 0, count);
    std::shared_ptr<Gui> created = getPlugin(L).createGui(tree, placement);
    listenToHandlers(L, *created, handlers);

    // The GUI is registered before it mounts, so listeners of the mount event already receive its userdata.
    lua::Userdata::emplace<Gui>(L, created);
    const int userdata = lua_gettop(L);
    pushRoot(L);
    lua_createtable(L, 0, 2);
    lua_pushvalue(L, userdata);
    lua_setfield(L, -2, "gui");
    lua_pushvalue(L, handlers);
    lua_setfield(L, -2, "handlers");
    lua_rawsetp(L, -2, created.get());
    lua_pop(L, 1);

    // A GUI owned by a scene of the stack belongs to that scene, so it shows and leaves with it.
    const std::shared_ptr<const core::Scene> scene = owner != 0 ? lua::ScriptedScene::find(L, owner) : nullptr;
    getPlugin(L).mount(created, layer, scene);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<MountLink>(getPlugin(L), created));
    }
    lua_pushvalue(L, userdata);
    return 1;
}

// Pushes the userdata of a GUI for Lua listeners of the GUI events, or `nil` for a GUI mounted from C++.
void UiLua::pushGui(lua_State* L, const std::shared_ptr<Gui>& gui) {
    pushRoot(L);
    if (lua_rawgetp(L, -1, gui.get()) == LUA_TTABLE) {
        lua_getfield(L, -1, "gui");
    } else {
        lua_pushnil(L);
    }
    lua_replace(L, -3);
    lua_pop(L, 1);
}

// Handlers change only after the GUI accepted the properties, so a failed set leaves them as they were.
int UiLua::guiSet(lua_State* L) {
    Gui& self = checkGui(L);
    const std::string id = lua::Stack::read<std::string>(L, 2);
    if (!pushHandlers(L, self)) {
        return luaL_error(L, "The GUI is not mounted.");
    }
    const int handlers = lua_gettop(L);
    lua_newtable(L);
    const int collected = lua_gettop(L);
    const Component* node = self.find(id);
    self.set(id, convertProperties(L, 3, collected, node != nullptr && node->getKind() == "collection"));

    lua_pushnil(L);
    while (lua_next(L, collected) != 0) {
        storeHandler(L, handlers, id, lua::Stack::read<std::string>(L, -2), lua_gettop(L));
        lua_pop(L, 1);
    }
    listenToEvents(L, self, id, collected);
    return 0;
}

// Handlers change only after the GUI accepted the new children. Then nodes that left the GUI or were built anew lose their old handlers and the new nodes get theirs.
int UiLua::guiReplaceChildren(lua_State* L) {
    Gui& self = checkGui(L);
    const std::string id = lua::Stack::read<std::string>(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!pushHandlers(L, self)) {
        return luaL_error(L, "The GUI is not mounted.");
    }
    const int handlers = lua_gettop(L);
    lua_newtable(L);
    const int collected = lua_gettop(L);

    core::Json children = core::Json::array();
    const auto length = static_cast<lua_Integer>(lua_rawlen(L, 3));
    std::size_t count = 0;
    for (lua_Integer child = 1; child <= length; ++child) {
        lua_rawgeti(L, 3, child);
        children.push_back(convertNode(L, -1, collected, 1, count));
        lua_pop(L, 1);
    }
    self.replaceChildren(id, children);
    CollectionLua::prune(L, self);

    std::vector<std::string> rebuilt;
    for (const core::Json& child : children) {
        collectIds(child, rebuilt);
    }
    for (const std::string& node : rebuilt) {
        lua_pushnil(L);
        lua_setfield(L, handlers, node.c_str());
    }
    pruneHandlers(L, handlers, self);
    lua_pushnil(L);
    while (lua_next(L, collected) != 0) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, handlers);
    }
    listenToHandlers(L, self, collected);
    return 0;
}

// Removes the handler of one event with `removeHandler(id, event)` and returns whether the node had one.
int UiLua::guiRemoveHandler(lua_State* L) {
    Gui& self = checkGui(L);
    const std::string id = lua::Stack::read<std::string>(L, 2);
    const std::string event = lua::Stack::read<std::string>(L, 3);
    if (!pushHandlers(L, self)) {
        return luaL_error(L, "The GUI is not mounted.");
    }
    if (self.find(id) == nullptr) {
        return luaL_error(L, "The GUI has no node with the id \"%s\".", id.c_str());
    }
    if (lua_getfield(L, -1, id.c_str()) != LUA_TTABLE) {
        lua::Stack::push(L, false);
        return 1;
    }
    const bool found = lua_getfield(L, -1, event.c_str()) == LUA_TFUNCTION;
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setfield(L, -2, event.c_str());
    if (const std::optional<Component::Notice> notice = Component::noticeFromName(event)) {
        self.listen(id, *notice, false);
    }
    lua::Stack::push(L, found);
    return 1;
}

int UiLua::guiGet(lua_State* L) {
    const core::Json* properties = checkGui(L).getProperties(lua::Stack::read<std::string_view>(L, 2));
    if (properties == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua::JsonConverter::push(L, *properties);
    return 1;
}

// Returns where a node was last drawn with `bounds(id)`, in the design coordinates of screen canvases, or `nil` before it was drawn.
int UiLua::guiBounds(lua_State* L) {
    const Component* component = checkGui(L).find(lua::Stack::read<std::string_view>(L, 2));
    if (component == nullptr || component->getBounds().isEmpty()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, getPlugin(L).getContext().toDesign(component->getBounds()));
    return 1;
}

int UiLua::guiHas(lua_State* L) {
    lua::Stack::push(L, checkGui(L).find(lua::Stack::read<std::string_view>(L, 2)) != nullptr);
    return 1;
}

int UiLua::guiCommand(lua_State* L) {
    Gui& self = checkGui(L);
    const core::Json arguments = lua_isnoneornil(L, 4) ? core::Json::object() : lua::JsonConverter::read(L, 4);
    self.command(getPlugin(L).getContext(), lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<std::string_view>(L, 3), arguments);
    return 0;
}

int UiLua::guiUnmount(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).unmount(checkGui(L)));
    return 1;
}

int UiLua::guiVisible(lua_State* L) {
    lua::Stack::push(L, checkGui(L).isVisible());
    return 1;
}

int UiLua::guiSetVisible(lua_State* L) {
    checkGui(L).setVisible(lua::Stack::read<bool>(L, 3));
    return 0;
}

int UiLua::guiMounted(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).isMounted(checkGui(L)));
    return 1;
}

// Returns the transform handle of the node with the id, whose offset, scale, opacity and tint tweens animate without running Lua.
int UiLua::guiTransform(lua_State* L) {
    const std::string id = lua::Stack::read<std::string>(L, 2);
    Component* component = checkGui(L).find(id);
    if (component == nullptr) {
        return luaL_error(L, "The GUI has no node with the id \"%s\".", id.c_str());
    }

    // The GUI keeps one handle per node, so a tween, which holds its target weakly, runs for as long as the GUI does.
    const std::string key = "transform " + id;
    lua::Userdata::pushField(L, 1, key.c_str());
    if (lua::Userdata::test<Transform>(L, -1) == component->getTransform().get()) {
        return 1;
    }
    lua_pop(L, 1);
    TransformLua::push(L, component->getTransform());
    lua::Userdata::setField(L, 1, key.c_str(), -1);
    return 1;
}

// Returns the handle of the collection with the id, the same one while the node exists.
int UiLua::guiCollection(lua_State* L) {
    const std::shared_ptr<Gui>& gui = lua::Userdata::checkShared<Gui>(L, 1);
    if (!getPlugin(L).isMounted(*gui)) {
        return luaL_error(L, "The GUI is not mounted.");
    }
    CollectionLua::push(L, gui, gui->getCollection(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int UiLua::guiPlacement(lua_State* L) {
    lua::Stack::push(L, checkGui(L).getPlacement() == Placement::Safe ? "safe" : "screen");
    return 1;
}

// Builds a node table with `node(kind, properties)`, the long form of `ui.<kind>{...}`.
int UiLua::node(lua_State* L) {
    const std::string kind = lua::Stack::read<std::string>(L, 1);
    if (!getPlugin(L).getComponents().contains(kind)) {
        return luaL_error(L, "There is no UI component kind named \"%s\".", kind.c_str());
    }
    if (lua_isnoneornil(L, 2)) {
        lua_newtable(L);
    } else {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushvalue(L, 2);
    }
    lua::Stack::push(L, kind);
    lua_setfield(L, -2, "kind");
    return 1;
}

int UiLua::kindBuilder(lua_State* L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    return node(L);
}

// Makes `ui.button{...}` and every other registered kind a shortcut for `node(kind, ...)`.
int UiLua::moduleIndex(lua_State* L) {
    const std::string_view key = lua::Stack::read<std::string_view>(L, 2);
    if (!getPlugin(L).getComponents().contains(key)) {
        return luaL_error(L, "The module \"haylen.ui\" has no member \"%s\".", std::string(key).c_str());
    }
    lua_pushvalue(L, 2);
    lua_pushcclosure(L, &lua::Binding::native<&kindBuilder>, 1);
    return 1;
}

int UiLua::setTheme(lua_State* L) {
    getPlugin(L).setTheme(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int UiLua::theme(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getTheme().getName());
    return 1;
}

int UiLua::themes(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getThemes());
    return 1;
}

// Loads a theme file with `loadTheme(path[, base])` and returns its name.
int UiLua::loadTheme(lua_State* L) {
    const std::string base = lua_isnoneornil(L, 2) ? std::string("dark") : lua::Stack::read<std::string>(L, 2);
    lua::Stack::push(L, getPlugin(L).loadTheme(lua::Runtime::getEngine(L), lua::Stack::read<std::string_view>(L, 1), base));
    return 1;
}

// Registers a theme from a table in the theme file format with `addTheme(definition[, base])` and returns its name.
int UiLua::addTheme(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const std::string base = lua_isnoneornil(L, 2) ? std::string("dark") : lua::Stack::read<std::string>(L, 2);
    lua::Stack::push(L, getPlugin(L).addTheme(lua::Runtime::getEngine(L), lua::JsonConverter::read(L, 1), base));
    return 1;
}

int UiLua::themeColor(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    const std::optional<Theme::Color> role = Theme::colorFromName(name);
    if (!role) {
        return luaL_error(L, "The theme has no color role named \"%s\".", name.c_str());
    }
    lua::Stack::push(L, getPlugin(L).getTheme().getColor(*role));
    return 1;
}

int UiLua::themeMetric(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    const std::optional<Theme::Metric> role = Theme::metricFromName(name);
    if (!role) {
        return luaL_error(L, "The theme has no metric named \"%s\".", name.c_str());
    }
    lua::Stack::push(L, getPlugin(L).getTheme().getMetric(*role));
    return 1;
}

int UiLua::themeImageFilter(lua_State* L) {
    lua::Stack::push(L, std::string(graphics::Texture::filterName(getPlugin(L).getTheme().getImageFilter())));
    return 1;
}

// Returns a font role of the active theme as `{font, size, bold, italic}`.
int UiLua::themeFont(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    const std::optional<Theme::Font> role = Theme::fontFromName(name);
    if (!role) {
        return luaL_error(L, "The theme has no font role named \"%s\".", name.c_str());
    }
    const Theme::FontStyle& style = getPlugin(L).getTheme().getFont(*role);
    lua_createtable(L, 0, 4);
    lua::Stack::push(L, style.font);
    lua_setfield(L, -2, "font");
    lua::Stack::push(L, style.size);
    lua_setfield(L, -2, "size");
    lua::Stack::push(L, style.bold);
    lua_setfield(L, -2, "bold");
    lua::Stack::push(L, style.italic);
    lua_setfield(L, -2, "italic");
    return 1;
}

// Returns the image of a surface of the active theme as `{slice, scale, padding = {top, right, bottom, left}, tint, colorize}`, or `nil` for a surface painted with flat colors.
int UiLua::themeSurface(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    const std::optional<Theme::Surface> role = Theme::surfaceFromName(name);
    if (!role) {
        return luaL_error(L, "The theme has no surface named \"%s\".", name.c_str());
    }
    const Theme::Image* image = getPlugin(L).getTheme().getSurface(*role);
    if (image == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 5);
    lua::Stack::push(L, image->slice);
    lua_setfield(L, -2, "slice");
    lua::Stack::push(L, image->scale);
    lua_setfield(L, -2, "scale");
    lua::Stack::push(L, std::vector<float>{image->padding.top, image->padding.right, image->padding.bottom, image->padding.left});
    lua_setfield(L, -2, "padding");
    lua::Stack::push(L, image->tint);
    lua_setfield(L, -2, "tint");
    lua::Stack::push(L, image->colorize);
    lua_setfield(L, -2, "colorize");
    return 1;
}

// Calls `listener(event)` for every event of every mounted GUI with `onEvent(listener[, {owner = scene}])` and returns the connection. A listener with an owner ends when the owner is released, such as a scene when it unloads.
int UiLua::onEvent(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kEventFields});
        if (lua_getfield(L, 2, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    core::Signal<Gui&, const Event&>::Options options;
    if (owner != 0) {
        options.owner = lua::Owners::getLifetime(L, owner);
    }
    auto function = std::make_shared<lua::Owners::Function>(L, 1, owner);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    core::Connection connection = getPlugin(L).events.connect([function, main](Gui& gui, const Event& event) {
        lua::Runtime::runReporting(main, [&] {
            const lua::StackScope scope(main);
            if (!function->push(main)) {
                return;
            }
            pushEventTable(main, gui, event);
            lua::Runtime::protectedCall(main, 1, 0);
        });
    }, std::move(options));
    // clang-format on

    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

// Registers a font with `addFont(name, path)` from a TrueType file, or with `addFont(name, family)` from a `FontFamily` or a `Font`.
int UiLua::addFont(lua_State* L) {
    if (lua_type(L, 2) == LUA_TSTRING) {
        getPlugin(L).addFont(lua::Runtime::getEngine(L), lua::Stack::read<std::string>(L, 1), lua::Stack::read<std::string_view>(L, 2));
        return 0;
    }
    getPlugin(L).addFontFamily(lua::Stack::read<std::string>(L, 1), graphics::FontLua::readFamily(L, 2));
    return 0;
}

int UiLua::usingPointer(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).isUsingPointer());
    return 1;
}

int UiLua::usingKeyboard(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).isUsingKeyboard());
    return 1;
}

// Returns the GUI and the node id that hold the focus with `focused()`, or `nil` when no mounted GUI holds it. A focused node without an id returns the GUI alone.
int UiLua::focused(lua_State* L) {
    plugins::UiPlugin& plugin = getPlugin(L);
    const std::shared_ptr<Gui> gui = plugin.findMounted(plugin.getFocus().getFocusedGui());
    if (!gui) {
        lua_pushnil(L);
        return 1;
    }
    pushGui(L, gui);
    const FocusNavigator& focus = plugin.getFocus();
    if (focus.getFocusedName().empty()) {
        return 1;
    }
    lua::Stack::push(L, focus.getFocusedName());
    if (focus.getFocusedItem().empty()) {
        return 2;
    }
    lua::Stack::push(L, focus.getFocusedItem());
    if (focus.getFocusedPart().empty()) {
        lua_pushnil(L);
    } else {
        lua::Stack::push(L, focus.getFocusedPart());
    }
    return 4;
}

int UiLua::focusOwner(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getFocus().getOwner());
    return 1;
}

int UiLua::clearFocus(lua_State* L) {
    plugins::UiPlugin& plugin = getPlugin(L);
    plugin.getBackend().makeCurrent();
    plugin.getFocus().clear();
    return 0;
}

int UiLua::focusRingVisible(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getFocus().isRingVisible());
    return 1;
}

int UiLua::safeAreaVisible(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).isSafeAreaVisible());
    return 1;
}

// Sets the direction of the whole UI with `setDirection('leftToRight', 'rightToLeft' or 'auto')`, where `auto` follows the direction the current language declares.
int UiLua::setDirection(lua_State* L) {
    getPlugin(L).setDirection(lua::Stack::read<text::Direction>(L, 1));
    return 0;
}

// Returns the direction set for the whole UI and the one it draws in, which differ when an automatic direction follows the language.
int UiLua::direction(lua_State* L) {
    plugins::UiPlugin& plugin = getPlugin(L);
    lua::Stack::push(L, plugin.getDirection());
    lua::Stack::push(L, plugin.getContext().getDirection());
    return 2;
}

// Sets how large the interface draws with `setScaleMode('design' or 'physical')`.
int UiLua::setScaleMode(lua_State* L) {
    const std::string_view name = lua::Stack::read<std::string_view>(L, 1);
    const std::optional<Scaling::Mode> mode = Scaling::modeFromName(name);
    if (!mode) {
        return luaL_error(L, "The UI scale mode \"%s\" is unknown. It is \"design\" or \"physical\".", std::string(name).c_str());
    }
    plugins::UiPlugin& plugin = getPlugin(L);
    Scaling scaling = plugin.getScaling();
    scaling.mode = *mode;
    plugin.setScaling(scaling);
    return 0;
}

int UiLua::scaleMode(lua_State* L) {
    lua::Stack::push(L, Scaling::modeName(getPlugin(L).getScaling().mode));
    return 1;
}

int UiLua::setScale(lua_State* L) {
    const auto factor = lua::Stack::read<float>(L, 1);
    if (!(factor >= Scaling::kMinimumFactor && factor <= Scaling::kMaximumFactor)) {
        return luaL_error(L, "The UI scale must be from 0.25 to 4.");
    }
    plugins::UiPlugin& plugin = getPlugin(L);
    Scaling scaling = plugin.getScaling();
    scaling.factor = factor;
    plugin.setScaling(scaling);
    return 0;
}

// Returns the factor `setScale` set and the design units one UI unit spans in the last frame, which the physical mode derives from the density of the screen.
int UiLua::scale(lua_State* L) {
    plugins::UiPlugin& plugin = getPlugin(L);
    lua::Stack::push(L, plugin.getScaling().factor);
    lua::Stack::push(L, plugin.getBackend().getScale());
    return 2;
}

int UiLua::setSafeAreaVisible(lua_State* L) {
    getPlugin(L).setSafeAreaVisible(lua::Stack::read<bool>(L, 1));
    return 0;
}

int UiLua::kinds(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getComponents().getKinds());
    return 1;
}

int UiLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"mount", &lua::Binding::native<&mount>}, {"node", &lua::Binding::native<&node>}, {"setTheme", &lua::Binding::native<&setTheme>}, {"theme", &lua::Binding::native<&theme>}, {"themes", &lua::Binding::native<&themes>}, {"loadTheme", &lua::Binding::native<&loadTheme>}, {"addTheme", &lua::Binding::native<&addTheme>}, {"themeColor", &lua::Binding::native<&themeColor>}, {"themeMetric", &lua::Binding::native<&themeMetric>}, {"themeFont", &lua::Binding::native<&themeFont>}, {"themeSurface", &lua::Binding::native<&themeSurface>}, {"themeImageFilter", &lua::Binding::native<&themeImageFilter>}, {"addFont", &lua::Binding::native<&addFont>}, {"usingPointer", &lua::Binding::native<&usingPointer>}, {"usingKeyboard", &lua::Binding::native<&usingKeyboard>}, {"focused", &lua::Binding::native<&focused>}, {"focusOwner", &lua::Binding::native<&focusOwner>}, {"clearFocus", &lua::Binding::native<&clearFocus>}, {"focusRingVisible", &lua::Binding::native<&focusRingVisible>}, {"safeAreaVisible", &lua::Binding::native<&safeAreaVisible>}, {"setSafeAreaVisible", &lua::Binding::native<&setSafeAreaVisible>}, {"setDirection", &lua::Binding::native<&setDirection>}, {"direction", &lua::Binding::native<&direction>}, {"setScaleMode", &lua::Binding::native<&setScaleMode>}, {"scaleMode", &lua::Binding::native<&scaleMode>}, {"setScale", &lua::Binding::native<&setScale>}, {"scale", &lua::Binding::native<&scale>}, {"kinds", &lua::Binding::native<&kinds>}, {"onEvent", &lua::Binding::native<&onEvent>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, &lua::Binding::native<&moduleIndex>);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    return 1;
}

void UiLua::install(lua_State* L) {
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kHandlersKey);
    core::EventsLua::addPayload<std::shared_ptr<Gui>>(&pushGui);
    TransformLua::install(L);
    CollectionLua::install(L);
    lua::ClassBuilder<Gui>(L).function("set", &lua::Binding::native<&guiSet>).function("replaceChildren", &lua::Binding::native<&guiReplaceChildren>).function("get", &lua::Binding::native<&guiGet>).function("has", &lua::Binding::native<&guiHas>).function("bounds", &lua::Binding::native<&guiBounds>).function("command", &lua::Binding::native<&guiCommand>).function("removeHandler", &lua::Binding::native<&guiRemoveHandler>).function("unmount", &lua::Binding::native<&guiUnmount>).property("visible", &guiVisible, &lua::Binding::native<&guiSetVisible>).property("mounted", &guiMounted).property("placement", &guiPlacement).function("transform", &lua::Binding::native<&guiTransform>).function("collection", &lua::Binding::native<&guiCollection>).install();
    lua::Binding::preload(L, "haylen.ui", &open);
}

void UiLua::deliverEvent(lua_State* L, Gui& gui, const Event& event) {
    const lua::StackScope scope(L);
    pushRoot(L);
    lua_rawgetp(L, -1, &gui);
    if (!lua_istable(L, -1)) {
        return;
    }
    lua_getfield(L, -1, "handlers");
    if (lua_getfield(L, -1, event.id.c_str()) != LUA_TTABLE || lua_getfield(L, -1, event.name.c_str()) != LUA_TFUNCTION) {
        return;
    }
    pushEventTable(L, gui, event);
    lua::Runtime::protectedCall(L, 1, 0);
}

// Listeners, tweens and timers that the GUI owns end with it.
void UiLua::forgetGui(lua_State* L, const Gui& gui) {
    const lua::StackScope scope(L);
    pushRoot(L);
    if (!lua_istable(L, -1)) {
        return;
    }
    const int root = lua_gettop(L);
    if (lua_rawgetp(L, root, &gui) == LUA_TTABLE && lua_getfield(L, -1, "gui") == LUA_TUSERDATA) {
        lua::Owners::release(L, -1);
    }
    lua_pushnil(L);
    lua_rawsetp(L, root, &gui);
    CollectionLua::forget(L, gui);
}

} // namespace haylen::ui
