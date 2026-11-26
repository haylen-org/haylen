#include "3d/nodes/Nodes3DBinding.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/FloatBufferBinding.hpp"
#include "haylen/3d/nodes/NodeTree3D.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/plugins/Nodes3DPlugin.hpp"

namespace haylen::lua::nodes3d {

using haylen::nodes3d::Component3D;
using haylen::nodes3d::ComponentType3D;
using haylen::nodes3d::Inheritance3D;
using haylen::nodes3d::Node3D;
using haylen::nodes3d::NodeEvent3D;
using haylen::nodes3d::NodeProperties3D;
using haylen::nodes3d::NodeTree3D;

NodeTree3D& Nodes3DBinding::checkTree(lua_State* L, int index) {
    return lua::Userdata::check<NodeTree3D>(L, index);
}

Node3D& Nodes3DBinding::checkNode(lua_State* L, int index) {
    return lua::Userdata::check<Node3D>(L, index);
}

bool Nodes3DBinding::pushTree(lua_State* L, const NodeTree3D* tree) {
    lua_getfield(L, LUA_REGISTRYINDEX, kTrees);
    lua_rawgetp(L, -1, tree);
    lua_remove(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    return true;
}

void Nodes3DBinding::pushNode(lua_State* L, const Node3D& node) {
    if (!node.isValid()) {
        lua_pushnil(L);
        return;
    }

    lua::Userdata::emplace<Node3D>(L, node);

    if (pushTree(L, &node.getTree())) {
        lua_setiuservalue(L, -2, 1);
    }
}

void Nodes3DBinding::pushNodes(lua_State* L, const std::vector<Node3D>& nodes) {
    lua_createtable(L, static_cast<int>(nodes.size()), 0);
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        pushNode(L, nodes[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

std::vector<Node3D> Nodes3DBinding::readNodes(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<Node3D> nodes;
    nodes.reserve(count);
    for (std::size_t place = 1; place <= count; ++place) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(place));
        nodes.push_back(checkNode(L, -1));
        lua_pop(L, 1);
    }

    return nodes;
}

bool Nodes3DBinding::pushDataTables(lua_State* L, const NodeTree3D* tree, bool create) {
    if (!pushTree(L, tree)) {
        return false;
    }

    lua::Userdata::pushField(L, -1, "data");
    if (!lua_isnil(L, -1) || !create) {
        lua_remove(L, -2);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return false;
        }

        return true;
    }

    lua_pop(L, 1);
    lua_newtable(L);
    lua::Userdata::setField(L, -2, "data", -1);
    lua_remove(L, -2);
    return true;
}

void Nodes3DBinding::forgetData(lua_State* L, const NodeTree3D* tree, std::uint64_t key) {
    if (!pushDataTables(L, tree, false)) {
        return;
    }

    lua_pushnil(L);
    lua_rawseti(L, -2, static_cast<lua_Integer>(key));
    lua_pop(L, 1);
}

// A copy takes a table of its own with the same keys and values, so the values themselves stay shared.
void Nodes3DBinding::copyData(lua_State* L, const NodeTree3D* tree, std::uint64_t source, std::uint64_t copy) {
    if (!pushDataTables(L, tree, false)) {
        return;
    }

    const int tables = lua_gettop(L);
    if (lua_rawgeti(L, tables, static_cast<lua_Integer>(source)) != LUA_TTABLE) {
        lua_pop(L, 2);
        return;
    }

    const int original = lua_gettop(L);
    lua_newtable(L);
    const int copied = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, original) != 0) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_rawset(L, copied);
    }

    lua_rawseti(L, tables, static_cast<lua_Integer>(copy));
    lua_pop(L, 2);
}

NodeProperties3D Nodes3DBinding::readProperties(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPropertyFields});
    NodeProperties3D properties;
    lua::Table::readField(L, index, "name", properties.name);
    lua::Table::readField(L, index, "position", properties.position);
    lua::Table::readField(L, index, "rotation", properties.rotation);
    lua::Table::readField(L, index, "scale", properties.scale);
    lua::Table::readField(L, index, "visible", properties.visible);
    lua::Table::readField(L, index, "enabled", properties.enabled);
    lua::Table::readField(L, index, "layers", properties.layers);
    lua::Table::readField(L, index, "static", properties.isStatic);
    lua::Table::readField(L, index, "frustumCulling", properties.frustumCulling);
    lua::Table::readField(L, index, "renderOrder", properties.renderOrder);
    lua::Table::readField(L, index, "inheritPosition", properties.inheritance.position);
    lua::Table::readField(L, index, "inheritRotation", properties.inheritance.rotation);
    lua::Table::readField(L, index, "inheritScale", properties.inheritance.scale);
    lua::Table::readField(L, index, "tags", properties.tags);
    return properties;
}

Node3D Nodes3DBinding::addNode(lua_State* L, NodeTree3D& tree, const Node3D& parent, int properties) {
    const int table = lua_absindex(L, properties);
    const NodeProperties3D read = readProperties(L, table);
    if (lua_getfield(L, table, "data") != LUA_TNIL) {
        lua::Table::requireType(L, "data", LUA_TTABLE);
    }

    lua_pop(L, 1);

    const Node3D node = parent.isValid() ? tree.add(parent, read) : tree.add(read);
    pushNode(L, node);

    if (lua_getfield(L, table, "data") != LUA_TNIL) {
        pushDataTables(L, &tree, true);
        lua_pushvalue(L, -2);
        lua_rawseti(L, -2, static_cast<lua_Integer>(node.getKey()));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    if (lua_getfield(L, table, "children") != LUA_TNIL) {
        lua::Table::requireType(L, "children", LUA_TTABLE);
        const int children = lua_gettop(L);
        const auto count = static_cast<lua_Integer>(lua_rawlen(L, children));
        for (lua_Integer child = 1; child <= count; ++child) {
            lua_rawgeti(L, children, child);
            (void)addNode(L, tree, node, -1);
            lua_pop(L, 2);
        }
    }

    lua_pop(L, 1);
    return node;
}

// A listener that a Lua call wakes runs on that call, so its error reaches the caller, and any other runs reported from the main thread.
void Nodes3DBinding::callListener(const lua::Owners::Function& function, lua_State* main, const Node3D& subject) {
    if (calling != nullptr) {
        if (function.push(calling)) {
            pushNode(calling, subject);
            lua_call(calling, 1, 0);
        }

        return;
    }

    // clang-format off
    lua::Runtime::runReporting(main, [&] {
        if (function.push(main)) {
            pushNode(main, subject);
            lua::Runtime::protectedCall(main, 1, 0);
        }
    });
    // clang-format on
}

// The tree keeps the data tables of its nodes in its user value, forgets the table of a node as it goes and gives the copy of a node a copy of its table.
int Nodes3DBinding::newTree(lua_State* L) {
    std::shared_ptr<NodeTree3D> tree = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Nodes3DPlugin>().createTree();
    const NodeTree3D* raw = tree.get();
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    tree->destroyed.connect([raw, main](const Node3D& node) {
        forgetData(calling != nullptr ? calling : main, raw, node.getKey());
    });
    tree->cloned.connect([raw, main](const Node3D& source, const Node3D& copy) {
        copyData(calling != nullptr ? calling : main, raw, source.getKey(), copy.getKey());
    });
    // clang-format on
    lua::Userdata::emplace<NodeTree3D>(L, std::move(tree));
    lua_getfield(L, LUA_REGISTRYINDEX, kTrees);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, raw);
    lua_pop(L, 1);
    return 1;
}

int Nodes3DBinding::treeAdd(lua_State* L) {
    NodeTree3D& tree = checkTree(L, 1);
    const CallScope scope(L);
    (void)addNode(L, tree, Node3D{}, 2);
    return 1;
}

int Nodes3DBinding::treeFind(lua_State* L) {
    pushNode(L, checkTree(L, 1).find(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

// Finds a node with `findByKey(key)` by the key of its handle, or returns nil once that node is gone.
int Nodes3DBinding::treeFindByKey(lua_State* L) {
    pushNode(L, checkTree(L, 1).findByKey(static_cast<std::uint64_t>(lua::Stack::read<lua_Integer>(L, 2))));
    return 1;
}

int Nodes3DBinding::treeRoots(lua_State* L) {
    pushNodes(L, checkTree(L, 1).getRoots());
    return 1;
}

int Nodes3DBinding::treeFindTagged(lua_State* L) {
    pushNodes(L, checkTree(L, 1).findTagged(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Nodes3DBinding::treeUpdate(lua_State* L) {
    checkTree(L, 1).update(&lua::Runtime::getEngine(L).getJobs());
    return 0;
}

int Nodes3DBinding::treeClear(lua_State* L) {
    NodeTree3D& tree = checkTree(L, 1);
    const CallScope scope(L);
    tree.clear();
    return 0;
}

// Moves many nodes from a float buffer with `writeTransforms(nodes, buffer[, first])`, seven floats for each node from the position `first`.
int Nodes3DBinding::treeWriteTransforms(lua_State* L) {
    NodeTree3D& tree = checkTree(L, 1);
    const std::vector<Node3D> nodes = readNodes(L, 2);
    const std::span<float> values = lua::Userdata::check<haylen::core::FloatBuffer>(L, 3).getValues();
    const lua_Integer first = luaL_optinteger(L, 4, 1);
    luaL_argcheck(L, first >= 1 && static_cast<std::size_t>(first) <= values.size() + 1, 4, "the first position is outside the buffer");
    tree.writeTransforms(nodes, values.subspan(static_cast<std::size_t>(first - 1)));
    return 0;
}

int Nodes3DBinding::treeReadTransforms(lua_State* L) {
    NodeTree3D& tree = checkTree(L, 1);
    const std::vector<Node3D> nodes = readNodes(L, 2);
    const std::span<float> values = lua::Userdata::check<haylen::core::FloatBuffer>(L, 3).getValues();
    const lua_Integer first = luaL_optinteger(L, 4, 1);
    luaL_argcheck(L, first >= 1 && static_cast<std::size_t>(first) <= values.size() + 1, 4, "the first position is outside the buffer");
    tree.readTransforms(nodes, values.subspan(static_cast<std::size_t>(first - 1)));
    return 0;
}

int Nodes3DBinding::treeSize(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).size());
    return 1;
}

int Nodes3DBinding::treeUpdatedCount(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).getUpdatedCount());
    return 1;
}

int Nodes3DBinding::nodeValid(lua_State* L) {
    lua::Stack::push(L, checkNode(L, 1).isValid());
    return 1;
}

int Nodes3DBinding::nodeKey(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(checkNode(L, 1).getKey()));
    return 1;
}

int Nodes3DBinding::nodePath(lua_State* L) {
    lua::Stack::push(L, checkNode(L, 1).getPath());
    return 1;
}

int Nodes3DBinding::nodeChildCount(lua_State* L) {
    lua::Stack::push(L, checkNode(L, 1).getChildCount());
    return 1;
}

// The first read of the data of a node builds its table from the JSON data that C++ gave it, such as the `extras` of a glTF node, and every later read returns that same table.
int Nodes3DBinding::nodeData(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    const haylen::core::Json& data = node.getData();
    pushDataTables(L, &node.getTree(), true);
    if (lua_rawgeti(L, -1, static_cast<lua_Integer>(node.getKey())) != LUA_TNIL) {
        return 1;
    }

    lua_pop(L, 1);
    if (data.is_null()) {
        lua_newtable(L);
    } else if (data.is_object() || data.is_array()) {
        lua::JsonConverter::push(L, data);
    } else {
        return luaL_error(L, "The data of the node \"%s\" is a single value, so Lua reads it as a table only when it is a JSON object or array.", node.getPath().c_str());
    }

    lua_pushvalue(L, -1);
    lua_rawseti(L, -3, static_cast<lua_Integer>(node.getKey()));
    return 1;
}

int Nodes3DBinding::nodeGetParent(lua_State* L) {
    pushNode(L, checkNode(L, 1).getParent());
    return 1;
}

int Nodes3DBinding::nodeSetParentProperty(lua_State* L) {
    Node3D& node = checkNode(L, 1);
    const Node3D parent = lua_isnil(L, 3) ? Node3D{} : checkNode(L, 3);
    const CallScope scope(L);
    node.setParent(parent);
    return 0;
}

int Nodes3DBinding::nodeGetEnabled(lua_State* L) {
    lua::Stack::push(L, checkNode(L, 1).isEnabled());
    return 1;
}

int Nodes3DBinding::nodeSetEnabled(lua_State* L) {
    Node3D& node = checkNode(L, 1);
    const bool value = lua::Stack::read<bool>(L, 3);
    const CallScope scope(L);
    node.setEnabled(value);
    return 0;
}

template <bool Inheritance3D::* Part> int Nodes3DBinding::nodeGetInherit(lua_State* L) {
    lua::Stack::push(L, checkNode(L, 1).getInheritance().*Part);
    return 1;
}

template <bool Inheritance3D::* Part> int Nodes3DBinding::nodeSetInherit(lua_State* L) {
    Node3D& node = checkNode(L, 1);
    Inheritance3D inheritance = node.getInheritance();
    inheritance.*Part = lua::Stack::read<bool>(L, 3);
    node.setInheritance(inheritance);
    return 0;
}

int Nodes3DBinding::nodeAdd(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    const CallScope scope(L);
    (void)addNode(L, node.getTree(), node, 2);
    return 1;
}

int Nodes3DBinding::nodeFind(lua_State* L) {
    pushNode(L, checkNode(L, 1).find(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Nodes3DBinding::nodeChildren(lua_State* L) {
    pushNodes(L, checkNode(L, 1).getChildren());
    return 1;
}

// Copies the node with `clone({parent = node})`, under its own parent unless the options name another, or the root with `parent = false`.
int Nodes3DBinding::nodeClone(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    Node3D parent = node.getParent();
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kCloneFields});
        const int type = lua_getfield(L, 2, "parent");
        if (type == LUA_TBOOLEAN && lua_toboolean(L, -1) == 0) {
            parent = Node3D{};
        } else if (type != LUA_TNIL) {
            parent = checkNode(L, -1);
        }

        lua_pop(L, 1);
    }

    const CallScope scope(L);
    pushNode(L, node.getTree().clone(node, parent));
    return 1;
}

// Moves the node with `setParent(parent, {keepWorld = true})`, to the root with a parent of `nil`.
int Nodes3DBinding::nodeSetParent(lua_State* L) {
    Node3D& node = checkNode(L, 1);
    const Node3D parent = lua_isnoneornil(L, 2) ? Node3D{} : checkNode(L, 2);
    bool keepWorld = false;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kParentFields});
        lua::Table::readField(L, 3, "keepWorld", keepWorld);
    }

    const CallScope scope(L);
    node.setParent(parent, keepWorld);
    return 0;
}

int Nodes3DBinding::nodeLookAt(lua_State* L) {
    Node3D& node = checkNode(L, 1);
    const haylen::math::Vec3 target = lua::Stack::read<haylen::math::Vec3>(L, 2);
    const haylen::math::Vec3 up = lua_isnoneornil(L, 3) ? haylen::math::Vec3::up() : lua::Stack::read<haylen::math::Vec3>(L, 3);
    node.lookAt(target, up);
    return 0;
}

int Nodes3DBinding::nodeDestroy(lua_State* L) {
    Node3D& node = checkNode(L, 1);
    const CallScope scope(L);
    node.destroy();
    return 0;
}

// Listens with `on(event, function, {owner = owner})`, called with the node, or with the child for `childAdded` and `childRemoved`. A listener ends with its owner, and without one it takes the default owner.
int Nodes3DBinding::nodeOn(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const std::optional<NodeEvent3D> event = haylen::core::EnumNames<NodeEvent3D>::fromName(name);
    if (!event) {
        return luaL_error(L, "Unknown node event \"%s\". Nodes report \"destroyed\", \"parentChanged\", \"childAdded\", \"childRemoved\" and \"enabledChanged\".", std::string(name).c_str());
    }

    const lua::Owners::Listener listener = lua::Owners::readListener(L, 3, 4);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    haylen::core::Connection connection = node.on(*event, [function = listener.function, main](const Node3D& subject) {
        callListener(*function, main, subject);
    }, {.owner = listener.lifetime});
    // clang-format on
    lua::Owners::pushConnection(L, listener, std::move(connection));
    return 1;
}

std::shared_ptr<const ComponentType3D> Nodes3DBinding::checkType(lua_State* L, int index) {
    const std::string_view name = lua::Stack::read<std::string_view>(L, index);
    std::shared_ptr<const ComponentType3D> type = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Nodes3DPlugin>().getComponentTypes().find(name);
    if (!type) {
        luaL_error(L, "No component type is named \"%s\". Register it in the component types of the 3D nodes plugin.", std::string(name).c_str());
    }

    return type;
}

haylen::core::Json Nodes3DBinding::readValue(lua_State* L, int index, const ComponentType3D::Property* property) {
    if (property != nullptr && property->kind == ComponentType3D::Kind::Vec3) {
        const haylen::math::Vec3 value = lua::Stack::read<haylen::math::Vec3>(L, index);
        return haylen::core::Json::array({value.x, value.y, value.z});
    }

    if (property != nullptr && property->kind == ComponentType3D::Kind::Quat) {
        const haylen::math::Quat value = lua::Stack::read<haylen::math::Quat>(L, index);
        return haylen::core::Json::array({value.x, value.y, value.z, value.w});
    }

    return lua::JsonConverter::read(L, index);
}

bool Nodes3DBinding::isNumbers(const haylen::core::Json& value, std::size_t count) {
    return value.is_array() && value.size() == count && std::ranges::all_of(value, &haylen::core::Json::is_number);
}

void Nodes3DBinding::pushValue(lua_State* L, const haylen::core::Json& value, const ComponentType3D::Property* property) {
    if (property != nullptr && property->kind == ComponentType3D::Kind::Vec3 && isNumbers(value, 3)) {
        lua::Stack::push(L, haylen::math::Vec3{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()});
        return;
    }

    if (property != nullptr && property->kind == ComponentType3D::Kind::Quat && isNumbers(value, 4)) {
        lua::Stack::push(L, haylen::math::Quat{value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()});
        return;
    }

    lua::JsonConverter::push(L, value);
}

// The pool of the type checks the keys and the values, so a key that the type lacks keeps its JSON form and the pool refuses it with its own sentence.
haylen::core::Json Nodes3DBinding::readOptions(lua_State* L, int index, const ComponentType3D& type) {
    haylen::core::Json options = haylen::core::Json::object();
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        if (lua_type(L, -2) != LUA_TSTRING) {
            luaL_error(L, "The options of a component are a table of its properties by name.");
        }

        const std::string key = lua::Stack::read<std::string>(L, -2);
        options[key] = readValue(L, -1, type.findProperty(key));
        lua_pop(L, 1);
    }

    return options;
}

void Nodes3DBinding::pushComponent(lua_State* L, const Component3D& component) {
    if (!component.isValid()) {
        lua_pushnil(L);
        return;
    }

    lua::Userdata::emplace<Component3D>(L, component);
}

int Nodes3DBinding::treeComponentCount(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).getComponents().size());
    return 1;
}

int Nodes3DBinding::treeGetProcessMode(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).getProcessMode());
    return 1;
}

int Nodes3DBinding::treeSetProcessMode(lua_State* L) {
    checkTree(L, 1).setProcessMode(lua::Stack::read<haylen::core::ProcessMode>(L, 3));
    return 0;
}

// Lists every registered type as `{name, phases, properties}`, each property as `{name, kind, default, description}`.
int Nodes3DBinding::componentTypes(lua_State* L) {
    const auto& types = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Nodes3DPlugin>().getComponentTypes().getTypes();
    lua_createtable(L, static_cast<int>(types.size()), 0);
    for (std::size_t index = 0; index < types.size(); ++index) {
        const ComponentType3D& type = *types[index];
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, type.getName());
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, std::vector<ComponentType3D::Phase>(type.getPhases().begin(), type.getPhases().end()));
        lua_setfield(L, -2, "phases");

        const std::span<const ComponentType3D::Property> properties = type.getProperties();
        lua_createtable(L, static_cast<int>(properties.size()), 0);
        for (std::size_t place = 0; place < properties.size(); ++place) {
            const ComponentType3D::Property& property = properties[place];
            lua_createtable(L, 0, 4);
            lua::Stack::push(L, property.name);
            lua_setfield(L, -2, "name");
            lua::Stack::push(L, property.kind);
            lua_setfield(L, -2, "kind");
            pushValue(L, property.defaultValue, &property);
            lua_setfield(L, -2, "default");
            lua::Stack::push(L, property.description);
            lua_setfield(L, -2, "description");
            lua_rawseti(L, -2, static_cast<lua_Integer>(place + 1));
        }

        lua_setfield(L, -2, "properties");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int Nodes3DBinding::nodeAddComponent(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    const std::shared_ptr<const ComponentType3D> type = checkType(L, 2);
    const haylen::core::Json options = readOptions(L, 3, *type);
    pushComponent(L, node.addComponent(type, options));
    return 1;
}

int Nodes3DBinding::nodeComponent(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    pushComponent(L, node.findComponent(checkType(L, 2)));
    return 1;
}

int Nodes3DBinding::nodeComponents(lua_State* L) {
    const std::vector<Component3D> components = checkNode(L, 1).getComponents();
    lua_createtable(L, static_cast<int>(components.size()), 0);
    for (std::size_t index = 0; index < components.size(); ++index) {
        pushComponent(L, components[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int Nodes3DBinding::nodeRemoveComponent(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    node.getTree().getComponents().remove(node, *checkType(L, 2));
    return 0;
}

int Nodes3DBinding::nodeSaveComponents(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    lua::JsonConverter::push(L, node.getTree().getComponents().save(node));
    return 1;
}

int Nodes3DBinding::nodeLoadComponents(lua_State* L) {
    const Node3D& node = checkNode(L, 1);
    const haylen::core::Json components = lua::JsonConverter::read(L, 2);
    node.getTree().getComponents().load(node, components.is_array() && components.empty() ? haylen::core::Json::object() : components, lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Nodes3DPlugin>().getComponentTypes());
    return 0;
}

int Nodes3DBinding::componentGet(lua_State* L) {
    const Component3D& component = lua::Userdata::check<Component3D>(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    pushValue(L, component.get(name), component.getType()->findProperty(name));
    return 1;
}

int Nodes3DBinding::componentSet(lua_State* L) {
    Component3D& component = lua::Userdata::check<Component3D>(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    component.set(name, readValue(L, 3, component.getType()->findProperty(name)));
    return 0;
}

int Nodes3DBinding::componentSave(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<Component3D>(L, 1).save());
    return 1;
}

int Nodes3DBinding::componentRemove(lua_State* L) {
    lua::Userdata::check<Component3D>(L, 1).remove();
    return 0;
}

int Nodes3DBinding::componentType(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Component3D>(L, 1).getType()->getName());
    return 1;
}

int Nodes3DBinding::componentNode(lua_State* L) {
    pushNode(L, lua::Userdata::check<Component3D>(L, 1).getNode());
    return 1;
}

int Nodes3DBinding::componentValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Component3D>(L, 1).isValid());
    return 1;
}

int Nodes3DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newTree", &newTree},
        {"componentTypes", &lua::Binding::native<&componentTypes>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Nodes3DBinding::install(lua_State* L) {
    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, kTrees);

    lua::ClassBuilder<NodeTree3D>(L).function("add", &lua::Binding::native<&treeAdd>).function("find", &lua::Binding::native<&treeFind>).function("findByKey", &lua::Binding::native<&treeFindByKey>).function("roots", &lua::Binding::native<&treeRoots>).function("findTagged", &lua::Binding::native<&treeFindTagged>).function("update", &lua::Binding::native<&treeUpdate>).function("clear", &lua::Binding::native<&treeClear>).function("writeTransforms", &lua::Binding::native<&treeWriteTransforms>).function("readTransforms", &lua::Binding::native<&treeReadTransforms>).property("size", &treeSize).property("updatedCount", &treeUpdatedCount).property("componentCount", &treeComponentCount).property("processMode", &lua::Binding::native<&treeGetProcessMode>, &lua::Binding::native<&treeSetProcessMode>).meta("__eq", &lua::Userdata::equal<NodeTree3D>).install();

    lua::ClassBuilder<Node3D>(L).accessor<&Node3D::getName, &Node3D::setName>("name").liveAccessor<&Node3D::getPosition, &Node3D::setPosition>("position").liveAccessor<&Node3D::getX, &Node3D::setX>("x").liveAccessor<&Node3D::getY, &Node3D::setY>("y").liveAccessor<&Node3D::getZ, &Node3D::setZ>("z").liveAccessor<&Node3D::getRotation, &Node3D::setRotation>("rotation").liveAccessor<&Node3D::getScale, &Node3D::setScale>("scale").accessor<&Node3D::getTransform, &Node3D::setTransform>("transform").accessor<&Node3D::isVisible, &Node3D::setVisible>("visible").property("enabled", &lua::Binding::native<&nodeGetEnabled>, &lua::Binding::native<&nodeSetEnabled>).accessor<&Node3D::getLayers, &Node3D::setLayers>("layers").accessor<&Node3D::isStatic, &Node3D::setStatic>("static").accessor<&Node3D::hasFrustumCulling, &Node3D::setFrustumCulling>("frustumCulling").accessor<&Node3D::getRenderOrder, &Node3D::setRenderOrder>("renderOrder").property("inheritPosition", &lua::Binding::native<&nodeGetInherit<&Inheritance3D::position>>, &lua::Binding::native<&nodeSetInherit<&Inheritance3D::position>>).property("inheritRotation", &lua::Binding::native<&nodeGetInherit<&Inheritance3D::rotation>>, &lua::Binding::native<&nodeSetInherit<&Inheritance3D::rotation>>).property("inheritScale", &lua::Binding::native<&nodeGetInherit<&Inheritance3D::scale>>, &lua::Binding::native<&nodeSetInherit<&Inheritance3D::scale>>).property("parent", &lua::Binding::native<&nodeGetParent>, &lua::Binding::native<&nodeSetParentProperty>).property("tags", &lua::Binding::method<&Node3D::getTags>).property("data", &lua::Binding::native<&nodeData>).property("valid", &nodeValid).property("key", &lua::Binding::native<&nodeKey>).property("path", &lua::Binding::native<&nodePath>).property("childCount", &lua::Binding::native<&nodeChildCount>).property("worldMatrix", &lua::Binding::method<&Node3D::getWorldMatrix>).liveAccessor<&Node3D::getWorldPosition, &Node3D::setWorldPosition>("worldPosition").liveAccessor<&Node3D::getWorldRotation, &Node3D::setWorldRotation>("worldRotation").property("worldScale", &lua::Binding::method<&Node3D::getWorldScale>).property("worldVisible", &lua::Binding::method<&Node3D::isWorldVisible>).property("worldEnabled", &lua::Binding::method<&Node3D::isWorldEnabled>).function("add", &lua::Binding::native<&nodeAdd>).function("find", &lua::Binding::native<&nodeFind>).function("children", &lua::Binding::native<&nodeChildren>).function("clone", &lua::Binding::native<&nodeClone>).function("setParent", &lua::Binding::native<&nodeSetParent>).function("addTag", &lua::Binding::method<&Node3D::addTag>).function("removeTag", &lua::Binding::method<&Node3D::removeTag>).function("hasTag", &lua::Binding::method<&Node3D::hasTag>).function("toWorld", &lua::Binding::method<&Node3D::toWorld>).function("toLocal", &lua::Binding::method<&Node3D::toLocal>).function("lookAt", &lua::Binding::native<&nodeLookAt>).function("destroy", &lua::Binding::native<&nodeDestroy>).function("on", &lua::Binding::native<&nodeOn>).function("addComponent", &lua::Binding::native<&nodeAddComponent>).function("component", &lua::Binding::native<&nodeComponent>).function("components", &lua::Binding::native<&nodeComponents>).function("removeComponent", &lua::Binding::native<&nodeRemoveComponent>).function("saveComponents", &lua::Binding::native<&nodeSaveComponents>).function("loadComponents", &lua::Binding::native<&nodeLoadComponents>).meta("__eq", &lua::Userdata::equal<Node3D>).install();

    lua::ClassBuilder<Component3D>(L).function("get", &lua::Binding::native<&componentGet>).function("set", &lua::Binding::native<&componentSet>).function("save", &lua::Binding::native<&componentSave>).function("remove", &lua::Binding::native<&componentRemove>).property("type", &lua::Binding::native<&componentType>).property("node", &lua::Binding::native<&componentNode>).property("valid", &lua::Binding::native<&componentValid>).meta("__eq", &lua::Userdata::equal<Component3D>).install();

    lua::Binding::preload(L, "haylen.nodes3d", &open);
}

} // namespace haylen::lua::nodes3d
