#include "2d/graphics/TileLayerLua.hpp"

#include <utility>
#include <vector>

#include "core/FloatBufferLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

int TileLayerLua::create(lua_State* L) {
    std::vector<graphics::Texture> textures;
    if (lua::Userdata::test<graphics::Texture>(L, 1) != nullptr) {
        textures.push_back(lua::Stack::read<graphics::Texture>(L, 1));
    } else {
        textures = lua::Stack::read<std::vector<graphics::Texture>>(L, 1);
    }
    TileLayer::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kOptionFields});
        lua::Table::readField(L, 2, "cellWidth", options.cellSize.x);
        lua::Table::readField(L, 2, "cellHeight", options.cellSize.y);
        lua::Table::readField(L, 2, "chunkSize", options.chunkSize);
    }
    lua::Userdata::emplace<TileLayer>(L, std::make_shared<TileLayer>(std::move(textures), options));
    return 1;
}

TileLayer& TileLayerLua::check(lua_State* L) {
    return lua::Userdata::check<TileLayer>(L, 1);
}

// Defines a frame with `defineFrame(id, {texture = 1, source = rect, offsetX = 0, offsetY = 0, width, height})`.
int TileLayerLua::defineFrame(lua_State* L) {
    TileLayer& layer = check(L);
    const auto id = static_cast<int>(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TTABLE);
    lua::Table::checkFields(L, 3, {kFrameFields});
    lua_Integer texture = 1;
    TileLayer::Frame frame;
    lua::Table::readField(L, 3, "texture", texture);
    luaL_argcheck(L, texture >= 1, 3, "textures are numbered from one");
    frame.texture = static_cast<std::size_t>(texture - 1);
    lua::Table::readField(L, 3, "source", frame.source);
    lua::Table::readField(L, 3, "offsetX", frame.offset.x);
    lua::Table::readField(L, 3, "offsetY", frame.offset.y);
    lua::Table::readField(L, 3, "width", frame.size.x);
    lua::Table::readField(L, 3, "height", frame.size.y);
    layer.defineFrame(id, frame);
    return 0;
}

int TileLayerLua::setCell(lua_State* L) {
    check(L).setCell(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3)), static_cast<int>(luaL_checkinteger(L, 4)));
    return 0;
}

int TileLayerLua::setCells(lua_State* L) {
    const core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 5);
    check(L).setCells(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3)), static_cast<int>(luaL_checkinteger(L, 4)), buffer.getValues());
    return 0;
}

int TileLayerLua::cell(lua_State* L) {
    lua_pushinteger(L, check(L).getCell(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3))));
    return 1;
}

int TileLayerLua::clear(lua_State* L) {
    check(L).clear();
    return 0;
}

// Draws with `draw(order, offsetX, offsetY)`.
int TileLayerLua::draw(lua_State* L) {
    TileLayer& layer = check(L);
    const DrawOrder order = lua::TypeConverter::readDrawOrder(L, 2);
    const math::Vec2 offset{static_cast<float>(luaL_optnumber(L, 3, 0.0)), static_cast<float>(luaL_optnumber(L, 4, 0.0))};
    layer.draw(lua::Runtime::getEngine(L).getRenderer2D(), order, offset);
    return 0;
}

int TileLayerLua::chunkCount(lua_State* L) {
    lua::Stack::push(L, check(L).getChunkCount());
    return 1;
}

int TileLayerLua::batchCount(lua_State* L) {
    lua::Stack::push(L, check(L).getBatchCount());
    return 1;
}

void TileLayerLua::install(lua_State* L) {
    lua::ClassBuilder<TileLayer>(L).function("defineFrame", &lua::Binding::native<&defineFrame>).function("setCell", &lua::Binding::native<&setCell>).function("setCells", &lua::Binding::native<&setCells>).function("cell", &lua::Binding::native<&cell>).function("clear", &lua::Binding::native<&clear>).function("draw", &lua::Binding::native<&draw>).property("chunkCount", &chunkCount).property("batchCount", &batchCount).install();
}

} // namespace haylen::graphics2d
