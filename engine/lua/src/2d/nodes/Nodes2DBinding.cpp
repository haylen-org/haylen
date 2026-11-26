#include "2d/nodes/Nodes2DBinding.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "2d/particles/Particles2DBinding.hpp"
#include "2d/physics/Physics2DBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "debug/PickRegistry.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
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
#include "haylen/plugins/Nodes2DPlugin.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::nodes2d {

haylen::nodes2d::NodeSerializer::Assets Nodes2DBinding::getAssets(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    // clang-format off
    return {
        .texture = [&engine](const std::string& path, haylen::graphics::Texture::Options options) { return engine.getAssets().texture(path, options); },
        .font = [&engine](const std::string& path) { return path.empty() ? engine.getDefaultFont() : engine.getAssets().font(path); },
        .particles = [&engine](const std::string& path) {
            const auto effect = std::static_pointer_cast<haylen::particles2d::Effect>(engine.getAssets().load("particles", path));
            return std::make_shared<haylen::particles2d::System>(*effect);
        },
    };
    // clang-format on
}

haylen::nodes2d::NodeTree& Nodes2DBinding::checkTree(lua_State* L, int index) {
    return lua::Userdata::check<haylen::nodes2d::NodeTree>(L, index);
}

bool Nodes2DBinding::pushTree(lua_State* L, const haylen::nodes2d::NodeTree* tree) {
    lua_getfield(L, LUA_REGISTRYINDEX, kTrees);
    lua_rawgetp(L, -1, tree);
    lua_remove(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    return true;
}

void Nodes2DBinding::pushNode(lua_State* L, const haylen::nodes2d::Node& node) {
    if (!node.isValid()) {
        lua_pushnil(L);
        return;
    }

    lua::Userdata::emplace<haylen::nodes2d::Node>(L, node);

    if (pushTree(L, &node.getTree())) {
        lua_setiuservalue(L, -2, 1);
    }
}

void Nodes2DBinding::pushNodes(lua_State* L, const std::vector<haylen::nodes2d::Node>& nodes) {
    lua_createtable(L, static_cast<int>(nodes.size()), 0);
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        pushNode(L, nodes[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

bool Nodes2DBinding::pushTreeTable(lua_State* L, int node, const char* field, bool create) {
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

void Nodes2DBinding::clearData(lua_State* L, const haylen::nodes2d::NodeTree* tree, std::uint64_t key) {
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
haylen::nodes2d::NodeContent Nodes2DBinding::readContent(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    haylen::nodes2d::NodeContent content;
    int shown = 0;
    if (lua_getfield(L, table, "sprite") != LUA_TNIL) {
        content.value = lua::Userdata::check<haylen::graphics2d::Sprite>(L, -1);
        ++shown;
    }

    lua_pop(L, 1);

    if (lua_getfield(L, table, "text") != LUA_TNIL) {
        const int text = lua_gettop(L);
        if (lua_type(L, text) == LUA_TSTRING) {
            content.value = haylen::nodes2d::NodeContent::Text{.font = lua::Runtime::getEngine(L).getDefaultFont(), .content = lua::Stack::read<std::string>(L, text)};
        } else {
            luaL_checktype(L, text, LUA_TTABLE);
            haylen::nodes2d::NodeContent::Text shownText{
                .style = lua::TypeConverter::readTextStyle(L, text, {kTextFields, lua::TypeConverter::kDrawOrderFields}),
                .order = lua::TypeConverter::readDrawOrder(L, text, {kTextFields, lua::TypeConverter::kTextStyleFields}),
            };
            lua::Table::readField(L, text, "text", shownText.content);
            shownText.font = lua_getfield(L, text, "font") != LUA_TNIL ? lua::Userdata::checkShared<haylen::text::Font>(L, -1) : lua::Runtime::getEngine(L).getDefaultFont();
            lua_pop(L, 1);
            content.value = std::move(shownText);
        }

        ++shown;
    }

    lua_pop(L, 1);

    if (lua_getfield(L, table, "frame") != LUA_TNIL) {
        const int frame = lua_gettop(L);
        luaL_checktype(L, frame, LUA_TTABLE);
        haylen::nodes2d::NodeContent::Frame shownFrame{.order = lua::TypeConverter::readDrawOrder(L, frame, {kFrameFields, kFrameColorFields})};
        if (lua_getfield(L, frame, "slice") == LUA_TNIL) {
            luaL_error(L, "A frame needs the nine-slice it draws in \"slice\".");
        }

        shownFrame.slice = lua::Userdata::check<haylen::graphics2d::NineSlice>(L, -1);
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
        } else if (lua::Userdata::test<haylen::particles2d::Emitter>(L, -1) != nullptr) {
            content.value = lua::Userdata::checkShared<haylen::particles2d::Emitter>(L, -1);
        } else {
            content.value = lua::Userdata::checkShared<haylen::particles2d::System>(L, -1);
        }

        ++shown;
    }

    lua_pop(L, 1);

    if (shown > 1) {
        luaL_error(L, "A node shows one of \"sprite\", \"text\", \"frame\" and \"particles\".");
    }

    return content;
}

haylen::nodes2d::NodeProperties Nodes2DBinding::readProperties(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPropertyFields});
    haylen::nodes2d::NodeProperties properties;
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

haylen::nodes2d::Node Nodes2DBinding::addNode(lua_State* L, haylen::nodes2d::NodeTree& tree, const haylen::nodes2d::Node& parent, int properties) {
    const int table = lua_absindex(L, properties);
    const haylen::nodes2d::Node node = parent.isValid() ? tree.add(parent, readProperties(L, table)) : tree.add(readProperties(L, table));
    pushNode(L, node);
    const int pushed = lua_gettop(L);

    if (lua_getfield(L, table, "body") != LUA_TNIL) {
        lua_setfield(L, pushed, "body");
    } else {
        lua_pop(L, 1);
    }

    if (lua_getfield(L, table, "data") != LUA_TNIL) {
        lua::Table::requireType(L, "data", LUA_TTABLE);
        pushTreeTable(L, pushed, "data", true);
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

std::vector<haylen::nodes2d::Node> Nodes2DBinding::readNodes(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<haylen::nodes2d::Node> nodes;
    nodes.reserve(count);
    for (std::size_t place = 1; place <= count; ++place) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(place));
        nodes.push_back(lua::Userdata::check<haylen::nodes2d::Node>(L, -1));
        lua_pop(L, 1);
    }

    return nodes;
}

// The tree keeps the data, the followed bodies and the listeners of its nodes in its user value, and forgets the data of a node as it goes.
int Nodes2DBinding::newTree(lua_State* L) {
    std::shared_ptr<haylen::nodes2d::NodeTree> tree = haylen::nodes2d::NodeTree::create();
    const haylen::nodes2d::NodeTree* raw = tree.get();
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    tree->destroyed.connect([raw, main](const haylen::nodes2d::Node& node) {
        clearData(destroying != nullptr ? destroying : main, raw, node.getKey());
    });
    // clang-format on
    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Nodes2DPlugin>().track(tree);
    lua::Userdata::emplace<haylen::nodes2d::NodeTree>(L, std::move(tree));
    lua_getfield(L, LUA_REGISTRYINDEX, kTrees);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, raw);
    lua_pop(L, 1);
    return 1;
}

int Nodes2DBinding::newPrefab(lua_State* L) {
    pushPrefab(L, std::make_shared<haylen::nodes2d::Prefab>(haylen::nodes2d::Prefab::parse(lua::JsonConverter::read(L, 1))));
    return 1;
}

int Nodes2DBinding::save(lua_State* L) {
    lua::JsonConverter::push(L, haylen::nodes2d::NodeSerializer::save(lua::Userdata::check<haylen::nodes2d::Node>(L, 1), lua::Runtime::getEngine(L).getDefaultFont()));
    return 1;
}

void Nodes2DBinding::pushPrefab(lua_State* L, std::shared_ptr<haylen::nodes2d::Prefab> prefab) {
    lua::Userdata::emplace<haylen::nodes2d::Prefab>(L, std::move(prefab));
}

int Nodes2DBinding::treeAdd(lua_State* L) {
    haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    (void)addNode(L, tree, haylen::nodes2d::Node{}, 2);
    return 1;
}

int Nodes2DBinding::treeInstantiate(lua_State* L) {
    haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    const haylen::nodes2d::Prefab& prefab = lua::Userdata::check<haylen::nodes2d::Prefab>(L, 2);
    const haylen::core::Json overrides = lua_isnoneornil(L, 3) ? haylen::core::Json::object() : lua::JsonConverter::read(L, 3);
    const haylen::nodes2d::Node parent = lua_isnoneornil(L, 4) ? haylen::nodes2d::Node{} : lua::Userdata::check<haylen::nodes2d::Node>(L, 4);
    pushNode(L, prefab.instantiate(tree, parent, overrides.is_array() && overrides.empty() ? haylen::core::Json::object() : overrides, getAssets(L)));
    return 1;
}

int Nodes2DBinding::treeLoad(lua_State* L) {
    haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    const haylen::core::Json description = lua::JsonConverter::read(L, 2);
    const haylen::nodes2d::Node parent = lua_isnoneornil(L, 3) ? haylen::nodes2d::Node{} : lua::Userdata::check<haylen::nodes2d::Node>(L, 3);
    pushNode(L, haylen::nodes2d::NodeSerializer::load(tree, parent, description, getAssets(L)));
    return 1;
}

int Nodes2DBinding::treeFind(lua_State* L) {
    pushNode(L, checkTree(L, 1).find(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

// Finds a node with `findByKey(key)` by the key of its handle, or returns nil once that node is gone.
int Nodes2DBinding::treeFindByKey(lua_State* L) {
    pushNode(L, checkTree(L, 1).findByKey(static_cast<std::uint64_t>(lua::Stack::read<lua_Integer>(L, 2))));
    return 1;
}

int Nodes2DBinding::treeRoots(lua_State* L) {
    pushNodes(L, checkTree(L, 1).getRoots());
    return 1;
}

int Nodes2DBinding::treeUpdate(lua_State* L) {
    haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    tree.update(lua::Stack::read<float>(L, 2), static_cast<float>(engine.getClock().getInterpolation()), &engine.getJobs());
    return 0;
}

// While a pick looks for its hit, the tree tells the inspector which Lua value its tags name.
int Nodes2DBinding::treeDraw(lua_State* L) {
    haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    haylen::graphics2d::Renderer& renderer = lua::Runtime::getEngine(L).getRenderer2D();
    if (renderer.isPickSearching()) {
        lua::debug::PickRegistry::remember(L, 1, lua::debug::PickRegistry::ownerOf(&tree), &lua::Binding::native<&pickNode>);
    }

    tree.draw(renderer);
    return 0;
}

int Nodes2DBinding::pickNode(lua_State* L) {
    pushNode(L, checkTree(L, 1).findByKey(static_cast<std::uint64_t>(lua_tointeger(L, 2))));
    return 1;
}

int Nodes2DBinding::treeClear(lua_State* L) {
    haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    const DestroyScope scope(L);
    tree.clear();
    return 0;
}

int Nodes2DBinding::treeWriteTransforms(lua_State* L) {
    haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    const std::vector<haylen::nodes2d::Node> nodes = readNodes(L, 2);
    const haylen::core::FloatBuffer& buffer = lua::Userdata::check<haylen::core::FloatBuffer>(L, 3);
    tree.writeTransforms(nodes, buffer.getValues().first(std::min(buffer.size(), nodes.size() * 3)));
    return 0;
}

int Nodes2DBinding::treeReadTransforms(lua_State* L) {
    const haylen::nodes2d::NodeTree& tree = checkTree(L, 1);
    const std::vector<haylen::nodes2d::Node> nodes = readNodes(L, 2);
    haylen::core::FloatBuffer& buffer = lua::Userdata::check<haylen::core::FloatBuffer>(L, 3);
    tree.readTransforms(nodes, buffer.getValues().first(std::min(buffer.size(), nodes.size() * 3)));
    return 0;
}

int Nodes2DBinding::treeSize(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).size());
    return 1;
}

int Nodes2DBinding::treeDrawnCount(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).getDrawnCount());
    return 1;
}

int Nodes2DBinding::treeCulledCount(lua_State* L) {
    lua::Stack::push(L, checkTree(L, 1).getCulledCount());
    return 1;
}

int Nodes2DBinding::nodeValid(lua_State* L) {
    lua::Stack::push(L, static_cast<const haylen::nodes2d::Node&>(lua::Userdata::check<haylen::nodes2d::Node>(L, 1)).isValid());
    return 1;
}

int Nodes2DBinding::nodeKey(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getKey()));
    return 1;
}

int Nodes2DBinding::nodePath(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getPath());
    return 1;
}

int Nodes2DBinding::nodeChildCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getChildCount());
    return 1;
}

int Nodes2DBinding::nodeContent(lua_State* L) {
    constexpr std::array<std::string_view, 6> kKinds{"none", "sprite", "text", "frame", "particles", "particles"};
    lua::Stack::push(L, kKinds[lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getContent().value.index()]);
    return 1;
}

int Nodes2DBinding::nodeGetParent(lua_State* L) {
    pushNode(L, lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getParent());
    return 1;
}

int Nodes2DBinding::nodeSetParent(lua_State* L) {
    haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    node.setParent(lua_isnil(L, 3) ? haylen::nodes2d::Node{} : lua::Userdata::check<haylen::nodes2d::Node>(L, 3));
    return 0;
}

int Nodes2DBinding::nodeGetSprite(lua_State* L) {
    lua::Userdata::emplace<haylen::graphics2d::Sprite>(L, lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getSprite());
    return 1;
}

int Nodes2DBinding::nodeSetSprite(lua_State* L) {
    lua::Userdata::check<haylen::nodes2d::Node>(L, 1).setContent({.value = lua::Userdata::check<haylen::graphics2d::Sprite>(L, 3)});
    return 0;
}

int Nodes2DBinding::nodeGetText(lua_State* L) {
    const auto* text = std::get_if<haylen::nodes2d::NodeContent::Text>(&lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getContent().value);
    if (text == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, text->content);
    return 1;
}

// Changing the text of a node keeps its font and style, and a node without text shows the text in the default font.
int Nodes2DBinding::nodeSetText(lua_State* L) {
    haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    haylen::nodes2d::NodeContent content = node.getContent();
    if (auto* text = std::get_if<haylen::nodes2d::NodeContent::Text>(&content.value)) {
        text->content = lua::Stack::read<std::string>(L, 3);
    } else {
        content = {.value = haylen::nodes2d::NodeContent::Text{.font = lua::Runtime::getEngine(L).getDefaultFont(), .content = lua::Stack::read<std::string>(L, 3)}};
    }

    node.setContent(std::move(content));
    return 0;
}

int Nodes2DBinding::nodeGetBody(lua_State* L) {
    const haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    if (!node.getFollowed().isValid() || !pushTreeTable(L, 1, "bodies", false)) {
        lua_pushnil(L);
        return 1;
    }

    lua_rawgeti(L, -1, static_cast<lua_Integer>(node.getKey()));
    return 1;
}

// The tree holds the body of a follower, which keeps its world alive while the node follows it.
int Nodes2DBinding::nodeSetBody(lua_State* L) {
    haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    if (lua_isnil(L, 3)) {
        node.follow({});
    } else {
        node.follow(lua::Userdata::check<lua::physics2d::ScriptedHandle<haylen::physics2d::Body>>(L, 3).handle);
    }

    pushTreeTable(L, 1, "bodies", true);
    lua_pushvalue(L, 3);
    lua_rawseti(L, -2, static_cast<lua_Integer>(node.getKey()));
    return 0;
}

int Nodes2DBinding::nodeData(lua_State* L) {
    const haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
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

int Nodes2DBinding::nodeAdd(lua_State* L) {
    const haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    (void)addNode(L, node.getTree(), node, 2);
    return 1;
}

int Nodes2DBinding::nodeFind(lua_State* L) {
    pushNode(L, lua::Userdata::check<haylen::nodes2d::Node>(L, 1).find(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Nodes2DBinding::nodeChildren(lua_State* L) {
    pushNodes(L, lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getChildren());
    return 1;
}

int Nodes2DBinding::nodeShow(lua_State* L) {
    haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    if (lua_isnoneornil(L, 2)) {
        node.setContent({});
        return 0;
    }

    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kContentFields});
    node.setContent(readContent(L, 2));
    return 0;
}

int Nodes2DBinding::nodeToWorld(lua_State* L) {
    const haylen::math::Vec2 point = lua::Userdata::check<haylen::nodes2d::Node>(L, 1).toWorld({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int Nodes2DBinding::nodeToLocal(lua_State* L) {
    const haylen::math::Vec2 point = lua::Userdata::check<haylen::nodes2d::Node>(L, 1).toLocal({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int Nodes2DBinding::nodeWorldTransform(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::nodes2d::Node>(L, 1).getWorldTransform());
    return 1;
}

int Nodes2DBinding::nodeDestroy(lua_State* L) {
    haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    const DestroyScope scope(L);
    node.destroy();
    return 0;
}

// A node that goes while Lua destroys nodes calls the listener on that call, so an error reaches its caller, and otherwise the main thread reports the error.
void Nodes2DBinding::notifyDestroyed(const lua::Owners::Function& function, lua_State* main, const haylen::nodes2d::Node& gone) {
    if (destroying != nullptr) {
        callDestroyed(function, destroying, gone, false);
        return;
    }

    lua::Runtime::runReporting(main, [&] { callDestroyed(function, main, gone, true); });
}

void Nodes2DBinding::callDestroyed(const lua::Owners::Function& function, lua_State* state, const haylen::nodes2d::Node& gone, bool reported) {
    if (!function.push(state)) {
        return;
    }

    pushNode(state, gone);

    if (reported) {
        lua::Runtime::protectedCall(state, 1, 0);
        return;
    }

    lua_call(state, 1, 0);
}

// Listens with `on('destroyed', function, {owner = owner})`, called with the node right before it goes. A listener ends with its owner, and without one it takes the default owner.
int Nodes2DBinding::nodeOn(lua_State* L) {
    const haylen::nodes2d::Node& node = lua::Userdata::check<haylen::nodes2d::Node>(L, 1);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "destroyed") {
        return luaL_error(L, "Unknown node event \"%s\". Nodes report \"destroyed\".", std::string(event).c_str());
    }

    const lua::Owners::Listener listener = lua::Owners::readListener(L, 3, 4);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    haylen::core::Connection connection = node.getTree().onDestroyed(node, [function = listener.function, main](const haylen::nodes2d::Node& gone) {
        notifyDestroyed(*function, main, gone);
    }, {.owner = listener.lifetime});
    // clang-format on
    lua::Owners::pushConnection(L, listener, std::move(connection));
    return 1;
}

int Nodes2DBinding::prefabDescription(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<haylen::nodes2d::Prefab>(L, 1).getDescription());
    return 1;
}

int Nodes2DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newTree", &newTree},
        {"newPrefab", &lua::Binding::native<&newPrefab>},
        {"save", &lua::Binding::native<&save>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Nodes2DBinding::install(lua_State* L) {
    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, kTrees);

    lua::ClassBuilder<haylen::nodes2d::NodeTree>(L).function("add", &lua::Binding::native<&treeAdd>).function("instantiate", &lua::Binding::native<&treeInstantiate>).function("load", &lua::Binding::native<&treeLoad>).function("find", &lua::Binding::native<&treeFind>).function("roots", &lua::Binding::native<&treeRoots>).function("findByKey", &lua::Binding::native<&treeFindByKey>).function("update", &lua::Binding::native<&treeUpdate>).function("draw", &lua::Binding::native<&treeDraw>).function("clear", &lua::Binding::native<&treeClear>).function("writeTransforms", &lua::Binding::native<&treeWriteTransforms>).function("readTransforms", &lua::Binding::native<&treeReadTransforms>).property("size", &treeSize).property("drawnCount", &treeDrawnCount).property("culledCount", &treeCulledCount).meta("__eq", &lua::Userdata::equal<haylen::nodes2d::NodeTree>).install();

    lua::ClassBuilder<haylen::nodes2d::Node>(L).accessor<&haylen::nodes2d::Node::getName, &haylen::nodes2d::Node::setName>("name").accessor<&haylen::nodes2d::Node::getX, &haylen::nodes2d::Node::setX>("x").accessor<&haylen::nodes2d::Node::getY, &haylen::nodes2d::Node::setY>("y").accessor<&haylen::nodes2d::Node::getPosition, &haylen::nodes2d::Node::setPosition>("position").accessor<&haylen::nodes2d::Node::getRotation, &haylen::nodes2d::Node::setRotation>("rotation").accessor<&haylen::nodes2d::Node::getScaleX, &haylen::nodes2d::Node::setScaleX>("scaleX").accessor<&haylen::nodes2d::Node::getScaleY, &haylen::nodes2d::Node::setScaleY>("scaleY").accessor<&haylen::nodes2d::Node::getColor, &haylen::nodes2d::Node::setColor>("color").accessor<&haylen::nodes2d::Node::isVisible, &haylen::nodes2d::Node::setVisible>("visible").accessor<&haylen::nodes2d::Node::getLayer, &haylen::nodes2d::Node::setLayer>("layer").accessor<&haylen::nodes2d::Node::getDepth, &haylen::nodes2d::Node::setDepth>("depth").accessor<&haylen::nodes2d::Node::getSortOffset, &haylen::nodes2d::Node::setSortOffset>("sortOffset").property("worldColor", &lua::Binding::method<&haylen::nodes2d::Node::getWorldColor>).property("worldLayer", &lua::Binding::method<&haylen::nodes2d::Node::getWorldLayer>).property("worldVisible", &lua::Binding::method<&haylen::nodes2d::Node::isWorldVisible>).property("valid", &nodeValid).property("key", &lua::Binding::native<&nodeKey>).property("path", &lua::Binding::native<&nodePath>).property("childCount", &lua::Binding::native<&nodeChildCount>).property("content", &lua::Binding::native<&nodeContent>).property("parent", &lua::Binding::native<&nodeGetParent>, &lua::Binding::native<&nodeSetParent>).property("sprite", &lua::Binding::native<&nodeGetSprite>, &lua::Binding::native<&nodeSetSprite>).property("text", &lua::Binding::native<&nodeGetText>, &lua::Binding::native<&nodeSetText>).property("body", &lua::Binding::native<&nodeGetBody>, &lua::Binding::native<&nodeSetBody>).property("data", &lua::Binding::native<&nodeData>).function("add", &lua::Binding::native<&nodeAdd>).function("find", &lua::Binding::native<&nodeFind>).function("children", &lua::Binding::native<&nodeChildren>).function("show", &lua::Binding::native<&nodeShow>).function("toWorld", &lua::Binding::native<&nodeToWorld>).function("toLocal", &lua::Binding::native<&nodeToLocal>).function("worldTransform", &lua::Binding::native<&nodeWorldTransform>).function("destroy", &lua::Binding::native<&nodeDestroy>).function("on", &lua::Binding::native<&nodeOn>).meta("__eq", &lua::Userdata::equal<haylen::nodes2d::Node>).install();

    lua::ClassBuilder<haylen::nodes2d::Prefab>(L).property("description", &lua::Binding::native<&prefabDescription>).meta("__eq", &lua::Userdata::equal<haylen::nodes2d::Prefab>).install();

    lua::Binding::preload(L, "haylen.nodes2d", &open);
}

} // namespace haylen::lua::nodes2d
