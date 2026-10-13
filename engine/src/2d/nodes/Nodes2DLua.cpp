#include "2d/nodes/Nodes2DLua.hpp"

#include <string>
#include <utility>
#include <vector>

#include "2d/particles/Particles2DLua.hpp"
#include "2d/physics/Physics2DLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "haylen/2d/nodes/NodeTree.hpp"
#include "haylen/2d/nodes/Prefab.hpp"
#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/System.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/Owners.hpp"

namespace haylen::nodes2d {

NodeSerializer::Assets Nodes2DLua::getAssets(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    // clang-format off
    return {
        .texture = [&engine](const std::string& path, graphics::Texture::Options options) { return engine.getAssets().texture(path, options); },
        .font = [&engine](const std::string& path) { return path.empty() ? engine.getDefaultFont() : engine.getAssets().font(path); },
        .particles = [&engine](const std::string& path) {
            const auto effect = std::static_pointer_cast<particles2d::Effect>(engine.getAssets().load("particles", path));
            return std::make_shared<particles2d::System>(*effect);
        },
    };
    // clang-format on
}

NodeTree& Nodes2DLua::checkTree(lua_State* L, int index) {
    return lua::Userdata::check<NodeTree>(L, index);
}

bool Nodes2DLua::pushTree(lua_State* L, const NodeTree* tree) {
    lua_getfield(L, LUA_REGISTRYINDEX, kTrees);
    lua_rawgetp(L, -1, tree);
    lua_remove(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

void Nodes2DLua::pushNode(lua_State* L, const Node& node) {
    if (!node.isValid()) {
        lua_pushnil(L);
        return;
    }
    lua::Userdata::emplace<Node>(L, node);
    if (pushTree(L, &node.getTree())) {
        lua_setiuservalue(L, -2, 1);
    }
}

void Nodes2DLua::pushNodes(lua_State* L, const std::vector<Node>& nodes) {
    lua_createtable(L, static_cast<int>(nodes.size()), 0);
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        pushNode(L, nodes[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

bool Nodes2DLua::pushTreeTable(lua_State* L, int node, const char* field, bool create) {
    const int position = lua_absindex(L, node);
    if (lua_getiuservalue(L, position, 1) != LUA_TUSERDATA) {
        lua_pop(L, 1);
        return false;
    }
    lua::Userdata::pushField(L, -1, field);
    lua_remove(L, -2);
    if (!lua_isnil(L, -1) || !create) {
        return !lua_isnil(L, -1);
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_getiuservalue(L, position, 1);
    lua::Userdata::setField(L, -1, field, -2);
    lua_pop(L, 1);
    return true;
}

void Nodes2DLua::clearData(lua_State* L, const NodeTree* tree, std::uint64_t key) {
    if (!pushTree(L, tree)) {
        return;
    }
    for (const char* field : {"data", "bodies"}) {
        lua::Userdata::pushField(L, -1, field);
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            lua_rawseti(L, -2, static_cast<lua_Integer>(key));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// A properties table shows at most one kind of content, and particles also come from the path of an effect file.
NodeContent Nodes2DLua::readContent(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    NodeContent content;
    int shown = 0;
    if (lua_getfield(L, table, "sprite") != LUA_TNIL) {
        content.value = lua::Userdata::check<graphics2d::Sprite>(L, -1);
        ++shown;
    }
    lua_pop(L, 1);

    if (lua_getfield(L, table, "text") != LUA_TNIL) {
        const int text = lua_gettop(L);
        if (lua_type(L, text) == LUA_TSTRING) {
            content.value = NodeContent::Text{.font = lua::Runtime::getEngine(L).getDefaultFont(), .content = lua::Stack::read<std::string>(L, text)};
        } else {
            luaL_checktype(L, text, LUA_TTABLE);
            NodeContent::Text shownText{
                .style = lua::TypeConverter::readTextStyle(L, text, {kTextFields, lua::TypeConverter::kDrawOrderFields}),
                .order = lua::TypeConverter::readDrawOrder(L, text, {kTextFields, lua::TypeConverter::kTextStyleFields}),
            };
            lua::Table::readField(L, text, "text", shownText.content);
            shownText.font = lua_getfield(L, text, "font") != LUA_TNIL ? lua::Userdata::checkShared<text::Font>(L, -1) : lua::Runtime::getEngine(L).getDefaultFont();
            lua_pop(L, 1);
            content.value = std::move(shownText);
        }
        ++shown;
    }
    lua_pop(L, 1);

    if (lua_getfield(L, table, "frame") != LUA_TNIL) {
        const int frame = lua_gettop(L);
        luaL_checktype(L, frame, LUA_TTABLE);
        NodeContent::Frame shownFrame{.order = lua::TypeConverter::readDrawOrder(L, frame, {kFrameFields, kFrameColorFields})};
        if (lua_getfield(L, frame, "slice") == LUA_TNIL) {
            luaL_error(L, "A frame needs the nine-slice it draws in \"slice\".");
        }
        shownFrame.slice = lua::Userdata::check<graphics2d::NineSlice>(L, -1);
        lua_pop(L, 1);
        lua::Table::readField(L, frame, "width", shownFrame.size.x);
        lua::Table::readField(L, frame, "height", shownFrame.size.y);
        lua::Table::readField(L, frame, "pivotX", shownFrame.pivot.x);
        lua::Table::readField(L, frame, "pivotY", shownFrame.pivot.y);
        lua::Table::readField(L, frame, "color", shownFrame.color);
        lua::Table::readField(L, frame, "borderScale", shownFrame.borderScale);
        content.value = std::move(shownFrame);
        ++shown;
    }
    lua_pop(L, 1);

    if (lua_getfield(L, table, "particles") != LUA_TNIL) {
        if (lua_type(L, -1) == LUA_TSTRING) {
            content.path = lua::Stack::read<std::string>(L, -1);
            content.value = getAssets(L).particles(content.path);
        } else if (lua::Userdata::test<particles2d::Emitter>(L, -1) != nullptr) {
            content.value = lua::Userdata::checkShared<particles2d::Emitter>(L, -1);
        } else {
            content.value = lua::Userdata::checkShared<particles2d::System>(L, -1);
        }
        ++shown;
    }
    lua_pop(L, 1);

    if (shown > 1) {
        luaL_error(L, "A node shows one of \"sprite\", \"text\", \"frame\" and \"particles\".");
    }
    return content;
}

NodeProperties Nodes2DLua::readProperties(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPropertyFields});
    NodeProperties properties;
    lua::Table::readField(L, index, "name", properties.name);
    lua::Table::readField(L, index, "x", properties.position.x);
    lua::Table::readField(L, index, "y", properties.position.y);
    lua::Table::readField(L, index, "rotation", properties.rotation);
    lua::Table::readField(L, index, "scaleX", properties.scale.x);
    lua::Table::readField(L, index, "scaleY", properties.scale.y);
    lua::Table::readField(L, index, "color", properties.color);
    lua::Table::readField(L, index, "visible", properties.visible);
    lua::Table::readField(L, index, "layer", properties.layer);
    lua::Table::readField(L, index, "depth", properties.depth);
    lua::Table::readField(L, index, "sortOffset", properties.sortOffset);
    properties.content = readContent(L, index);
    return properties;
}

Node Nodes2DLua::addNode(lua_State* L, NodeTree& tree, const Node& parent, int properties) {
    const int table = lua_absindex(L, properties);
    const Node node = parent.isValid() ? tree.add(parent, readProperties(L, table)) : tree.add(readProperties(L, table));
    pushNode(L, node);
    const int pushed = lua_gettop(L);

    if (lua_getfield(L, table, "body") != LUA_TNIL) {
        lua_setfield(L, pushed, "body");
    } else {
        lua_pop(L, 1);
    }
    if (lua_getfield(L, table, "data") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        pushTreeTable(L, pushed, "data", true);
        lua_pushvalue(L, -2);
        lua_rawseti(L, -2, static_cast<lua_Integer>(node.getKey()));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    if (lua_getfield(L, table, "children") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int children = lua_gettop(L);
        const auto count = static_cast<lua_Integer>(luaL_len(L, children));
        for (lua_Integer child = 1; child <= count; ++child) {
            lua_rawgeti(L, children, child);
            (void)addNode(L, tree, node, -1);
            lua_pop(L, 2);
        }
    }
    lua_pop(L, 1);
    return node;
}

std::vector<Node> Nodes2DLua::readNodes(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(luaL_len(L, index));
    std::vector<Node> nodes;
    nodes.reserve(count);
    for (std::size_t place = 1; place <= count; ++place) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(place));
        nodes.push_back(lua::Userdata::check<Node>(L, -1));
        lua_pop(L, 1);
    }
    return nodes;
}

// The tree keeps the data, the followed bodies and the listeners of its nodes in its user value, and forgets the data of a node as it goes.
int Nodes2DLua::newTree(lua_State* L) {
    std::shared_ptr<NodeTree> tree = NodeTree::create();
    const NodeTree* raw = tree.get();
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    tree->destroyed.connect([raw, main](const Node& node) {
        clearData(destroying != nullptr ? destroying : main, raw, node.getKey());
    });
    // clang-format on
    lua::Userdata::emplace<NodeTree>(L, std::move(tree));
    lua_getfield(L, LUA_REGISTRYINDEX, kTrees);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, raw);
    lua_pop(L, 1);
    return 1;
}

int Nodes2DLua::newPrefab(lua_State* L) {
    pushPrefab(L, std::make_shared<Prefab>(Prefab::parse(lua::JsonConverter::read(L, 1))));
    return 1;
}

int Nodes2DLua::save(lua_State* L) {
    lua::JsonConverter::push(L, NodeSerializer::save(lua::Userdata::check<Node>(L, 1), lua::Runtime::getEngine(L).getDefaultFont()));
    return 1;
}

void Nodes2DLua::pushPrefab(lua_State* L, std::shared_ptr<Prefab> prefab) {
    lua::Userdata::emplace<Prefab>(L, std::move(prefab));
}

int Nodes2DLua::treeAdd(lua_State* L) {
    NodeTree& tree = checkTree(L, 1);
    (void)addNode(L, tree, Node{}, 2);
    return 1;
}

int Nodes2DLua::treeInstantiate(lua_State* L) {
    NodeTree& tree = checkTree(L, 1);
    const Prefab& prefab = lua::Userdata::check<Prefab>(L, 2);
    const core::Json overrides = lua_isnoneornil(L, 3) ? core::Json::object() : lua::JsonConverter::read(L, 3);
    const Node parent = lua_isnoneornil(L, 4) ? Node{} : lua::Userdata::check<Node>(L, 4);
    pushNode(L, prefab.instantiate(tree, parent, overrides.is_array() && overrides.empty() ? core::Json::object() : overrides, getAssets(L)));
    return 1;
}

int Nodes2DLua::treeLoad(lua_State* L) {
    NodeTree& tree = checkTree(L, 1);
    const core::Json description = lua::JsonConverter::read(L, 2);
    const Node parent = lua_isnoneornil(L, 3) ? Node{} : lua::Userdata::check<Node>(L, 3);
    pushNode(L, NodeSerializer::load(tree, parent, description, getAssets(L)));
    return 1;
}

int Nodes2DLua::treeFind(lua_State* L) {
    pushNode(L, checkTree(L, 1).find(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Nodes2DLua::treeRoots(lua_State* L) {
    pushNodes(L, checkTree(L, 1).getRoots());
    return 1;
}

int Nodes2DLua::treeUpdate(lua_State* L) {
    NodeTree& tree = checkTree(L, 1);
    core::Engine& engine = lua::Runtime::getEngine(L);
    tree.update(lua::Stack::read<float>(L, 2), static_cast<float>(engine.getClock().getInterpolation()), &engine.getJobs());
    return 0;
}

int Nodes2DLua::treeDraw(lua_State* L) {
    checkTree(L, 1).draw(lua::Runtime::getEngine(L).getRenderer2D());
    return 0;
}

int Nodes2DLua::treeClear(lua_State* L) {
    NodeTree& tree = checkTree(L, 1);
    const DestroyScope scope(L);
    tree.clear();
    return 0;
}

int Nodes2DLua::treeWriteTransforms(lua_State* L) {
    NodeTree& tree = checkTree(L, 1);
    const std::vector<Node> nodes = readNodes(L, 2);
    const core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 3);
    tree.writeTransforms(nodes, buffer.getValues().first(std::min(buffer.size(), nodes.size() * 3)));
    return 0;
}

int Nodes2DLua::treeReadTransforms(lua_State* L) {
    const NodeTree& tree = checkTree(L, 1);
    const std::vector<Node> nodes = readNodes(L, 2);
    core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 3);
    tree.readTransforms(nodes, buffer.getValues().first(std::min(buffer.size(), nodes.size() * 3)));
    return 0;
}

int Nodes2DLua::treeSize(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).size());
    return 1;
}

int Nodes2DLua::treeDrawnCount(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).getDrawnCount());
    return 1;
}

int Nodes2DLua::treeCulledCount(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).getCulledCount());
    return 1;
}

int Nodes2DLua::nodeValid(lua_State* L) {
    lua::Stack::push(L, static_cast<const Node&>(lua::Userdata::check<Node>(L, 1)).isValid());
    return 1;
}

int Nodes2DLua::nodeKey(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<Node>(L, 1).getKey()));
    return 1;
}

int Nodes2DLua::nodePath(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Node>(L, 1).getPath());
    return 1;
}

int Nodes2DLua::nodeChildCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Node>(L, 1).getChildCount());
    return 1;
}

int Nodes2DLua::nodeContent(lua_State* L) {
    constexpr std::array<std::string_view, 6> kKinds{"none", "sprite", "text", "frame", "particles", "particles"};
    lua::Stack::push(L, kKinds[lua::Userdata::check<Node>(L, 1).getContent().value.index()]);
    return 1;
}

int Nodes2DLua::nodeGetParent(lua_State* L) {
    pushNode(L, lua::Userdata::check<Node>(L, 1).getParent());
    return 1;
}

int Nodes2DLua::nodeSetParent(lua_State* L) {
    Node& node = lua::Userdata::check<Node>(L, 1);
    node.setParent(lua_isnil(L, 3) ? Node{} : lua::Userdata::check<Node>(L, 3));
    return 0;
}

int Nodes2DLua::nodeGetSprite(lua_State* L) {
    lua::Userdata::emplace<graphics2d::Sprite>(L, lua::Userdata::check<Node>(L, 1).getSprite());
    return 1;
}

int Nodes2DLua::nodeSetSprite(lua_State* L) {
    lua::Userdata::check<Node>(L, 1).setContent({.value = lua::Userdata::check<graphics2d::Sprite>(L, 3)});
    return 0;
}

int Nodes2DLua::nodeGetText(lua_State* L) {
    const auto* text = std::get_if<NodeContent::Text>(&lua::Userdata::check<Node>(L, 1).getContent().value);
    if (text == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, text->content);
    return 1;
}

// Changing the text of a node keeps its font and style, and a node without text shows the text in the default font.
int Nodes2DLua::nodeSetText(lua_State* L) {
    Node& node = lua::Userdata::check<Node>(L, 1);
    NodeContent content = node.getContent();
    if (auto* text = std::get_if<NodeContent::Text>(&content.value)) {
        text->content = lua::Stack::read<std::string>(L, 3);
    } else {
        content = {.value = NodeContent::Text{.font = lua::Runtime::getEngine(L).getDefaultFont(), .content = lua::Stack::read<std::string>(L, 3)}};
    }
    node.setContent(std::move(content));
    return 0;
}

int Nodes2DLua::nodeGetBody(lua_State* L) {
    const Node& node = lua::Userdata::check<Node>(L, 1);
    if (!node.getFollowed().isValid() || !pushTreeTable(L, 1, "bodies", false)) {
        lua_pushnil(L);
        return 1;
    }
    lua_rawgeti(L, -1, static_cast<lua_Integer>(node.getKey()));
    return 1;
}

// The tree holds the body of a follower, which keeps its world alive while the node follows it.
int Nodes2DLua::nodeSetBody(lua_State* L) {
    Node& node = lua::Userdata::check<Node>(L, 1);
    if (lua_isnil(L, 3)) {
        node.follow({});
    } else {
        node.follow(lua::Userdata::check<physics2d::ScriptedHandle<physics2d::Body>>(L, 3).handle);
    }
    pushTreeTable(L, 1, "bodies", true);
    lua_pushvalue(L, 3);
    lua_rawseti(L, -2, static_cast<lua_Integer>(node.getKey()));
    return 0;
}

int Nodes2DLua::nodeData(lua_State* L) {
    const Node& node = lua::Userdata::check<Node>(L, 1);
    (void)node.getTree();
    pushTreeTable(L, 1, "data", true);
    if (lua_rawgeti(L, -1, static_cast<lua_Integer>(node.getKey())) == LUA_TNIL) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawseti(L, -3, static_cast<lua_Integer>(node.getKey()));
    }
    return 1;
}

int Nodes2DLua::nodeAdd(lua_State* L) {
    const Node& node = lua::Userdata::check<Node>(L, 1);
    (void)addNode(L, node.getTree(), node, 2);
    return 1;
}

int Nodes2DLua::nodeFind(lua_State* L) {
    pushNode(L, lua::Userdata::check<Node>(L, 1).find(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Nodes2DLua::nodeChildren(lua_State* L) {
    pushNodes(L, lua::Userdata::check<Node>(L, 1).getChildren());
    return 1;
}

int Nodes2DLua::nodeShow(lua_State* L) {
    Node& node = lua::Userdata::check<Node>(L, 1);
    if (lua_isnoneornil(L, 2)) {
        node.setContent({});
        return 0;
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kContentFields});
    node.setContent(readContent(L, 2));
    return 0;
}

int Nodes2DLua::nodeToWorld(lua_State* L) {
    const math::Vec2 point = lua::Userdata::check<Node>(L, 1).toWorld({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int Nodes2DLua::nodeToLocal(lua_State* L) {
    const math::Vec2 point = lua::Userdata::check<Node>(L, 1).toLocal({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int Nodes2DLua::nodeWorldTransform(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Node>(L, 1).getWorldTransform());
    return 1;
}

int Nodes2DLua::nodeDestroy(lua_State* L) {
    Node& node = lua::Userdata::check<Node>(L, 1);
    const DestroyScope scope(L);
    node.destroy();
    return 0;
}

// Listens with `on('destroyed', function, {owner = owner})`, called with the node right before it goes. A listener ends with its owner, and without one it takes the default owner.
int Nodes2DLua::nodeOn(lua_State* L) {
    const Node& node = lua::Userdata::check<Node>(L, 1);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "destroyed") {
        return luaL_error(L, "Unknown node event \"%s\". Nodes report \"destroyed\".", std::string(event).c_str());
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kListenerFields});
        if (lua_getfield(L, 4, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    const std::weak_ptr<const void> lifetime = owner != 0 ? lua::Owners::getLifetime(L, owner) : std::weak_ptr<const void>();
    auto function = std::make_shared<lua::Owners::Function>(L, 3, owner);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    core::Connection connection = node.getTree().onDestroyed(node, [function, main](const Node& gone) {
        if (destroying == nullptr) {
            lua::Runtime::runReporting(main, [&] {
                if (function->push(main)) {
                    pushNode(main, gone);
                    lua::Runtime::protectedCall(main, 1, 0);
                }
            });
            return;
        }
        if (function->push(destroying)) {
            pushNode(destroying, gone);
            lua_call(destroying, 1, 0);
        }
    }, {.owner = lifetime});
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

int Nodes2DLua::prefabDescription(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<Prefab>(L, 1).getDescription());
    return 1;
}

int Nodes2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newTree", &newTree},
        {"newPrefab", &lua::Binding::native<&newPrefab>},
        {"save", &lua::Binding::native<&save>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Nodes2DLua::install(lua_State* L) {
    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, kTrees);

    lua::ClassBuilder<NodeTree>(L).function("add", &lua::Binding::native<&treeAdd>).function("instantiate", &lua::Binding::native<&treeInstantiate>).function("load", &lua::Binding::native<&treeLoad>).function("find", &lua::Binding::native<&treeFind>).function("roots", &lua::Binding::native<&treeRoots>).function("update", &lua::Binding::native<&treeUpdate>).function("draw", &lua::Binding::native<&treeDraw>).function("clear", &lua::Binding::native<&treeClear>).function("writeTransforms", &lua::Binding::native<&treeWriteTransforms>).function("readTransforms", &lua::Binding::native<&treeReadTransforms>).property("size", &treeSize).property("drawnCount", &treeDrawnCount).property("culledCount", &treeCulledCount).meta("__eq", &lua::Userdata::equal<NodeTree>).install();

    lua::ClassBuilder<Node>(L).accessor<&Node::getName, &Node::setName>("name").accessor<&Node::getX, &Node::setX>("x").accessor<&Node::getY, &Node::setY>("y").accessor<&Node::getPosition, &Node::setPosition>("position").accessor<&Node::getRotation, &Node::setRotation>("rotation").accessor<&Node::getScaleX, &Node::setScaleX>("scaleX").accessor<&Node::getScaleY, &Node::setScaleY>("scaleY").accessor<&Node::getColor, &Node::setColor>("color").accessor<&Node::isVisible, &Node::setVisible>("visible").accessor<&Node::getLayer, &Node::setLayer>("layer").accessor<&Node::getDepth, &Node::setDepth>("depth").accessor<&Node::getSortOffset, &Node::setSortOffset>("sortOffset").property("worldColor", &lua::Binding::method<&Node::getWorldColor>).property("worldLayer", &lua::Binding::method<&Node::getWorldLayer>).property("worldVisible", &lua::Binding::method<&Node::isWorldVisible>).property("valid", &nodeValid).property("key", &lua::Binding::native<&nodeKey>).property("path", &lua::Binding::native<&nodePath>).property("childCount", &lua::Binding::native<&nodeChildCount>).property("content", &lua::Binding::native<&nodeContent>).property("parent", &lua::Binding::native<&nodeGetParent>, &lua::Binding::native<&nodeSetParent>).property("sprite", &lua::Binding::native<&nodeGetSprite>, &lua::Binding::native<&nodeSetSprite>).property("text", &lua::Binding::native<&nodeGetText>, &lua::Binding::native<&nodeSetText>).property("body", &lua::Binding::native<&nodeGetBody>, &lua::Binding::native<&nodeSetBody>).property("data", &lua::Binding::native<&nodeData>).function("add", &lua::Binding::native<&nodeAdd>).function("find", &lua::Binding::native<&nodeFind>).function("children", &lua::Binding::native<&nodeChildren>).function("show", &lua::Binding::native<&nodeShow>).function("toWorld", &lua::Binding::native<&nodeToWorld>).function("toLocal", &lua::Binding::native<&nodeToLocal>).function("worldTransform", &lua::Binding::native<&nodeWorldTransform>).function("destroy", &lua::Binding::native<&nodeDestroy>).function("on", &lua::Binding::native<&nodeOn>).meta("__eq", &lua::Userdata::equal<Node>).install();

    lua::ClassBuilder<Prefab>(L).property("description", &lua::Binding::native<&prefabDescription>).meta("__eq", &lua::Userdata::equal<Prefab>).install();

    lua::Binding::preload(L, "haylen.nodes2d", &open);
}

} // namespace haylen::nodes2d
