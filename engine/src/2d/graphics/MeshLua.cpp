#include "2d/graphics/MeshLua.hpp"

#include <lua.hpp>

#include <optional>
#include <utility>

#include "2d/graphics/StrokeLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

void MeshLua::readVertex(lua_State* L, int index, MeshVertex& vertex) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kVertexFields});
    lua::Table::readField(L, index, "x", vertex.position.x);
    lua::Table::readField(L, index, "y", vertex.position.y);
    lua::Table::readField(L, index, "u", vertex.uv.x);
    lua::Table::readField(L, index, "v", vertex.uv.y);
    lua::Table::readField(L, index, "color", vertex.color);
}

void MeshLua::readVertices(lua_State* L, int index, std::vector<MeshVertex>& vertices) {
    luaL_checktype(L, index, LUA_TTABLE);
    vertices.assign(static_cast<std::size_t>(luaL_len(L, index)), MeshVertex{});
    for (std::size_t place = 0; place < vertices.size(); ++place) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(place + 1));
        readVertex(L, lua_gettop(L), vertices[place]);
        lua_pop(L, 1);
    }
}

void MeshLua::readIndices(lua_State* L, int index, std::vector<std::uint32_t>& indices) {
    indices = lua::Stack::read<std::vector<std::uint32_t>>(L, index);
    for (std::uint32_t& entry : indices) {
        luaL_argcheck(L, entry >= 1, index, "mesh indices start at 1");
        --entry;
    }
}

int MeshLua::create(lua_State* L) {
    graphics::Texture texture = lua_isnil(L, 1) ? graphics::Texture{} : lua::Stack::read<graphics::Texture>(L, 1);
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    readVertices(L, 2, vertices);
    readIndices(L, 3, indices);
    lua::Userdata::emplace<Mesh>(L, std::make_shared<Mesh>(std::move(texture), std::move(vertices), std::move(indices)));
    return 1;
}

int MeshLua::createGrid(lua_State* L) {
    graphics::Texture texture = lua_isnil(L, 1) ? graphics::Texture{} : lua::Stack::read<graphics::Texture>(L, 1);
    const auto columns = static_cast<int>(luaL_checkinteger(L, 2));
    const auto rows = static_cast<int>(luaL_checkinteger(L, 3));
    math::Rect area{0.0F, 0.0F, texture.isValid() ? texture.getSize().x : 1.0F, texture.isValid() ? texture.getSize().y : 1.0F};
    std::optional<math::Rect> source;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kGridFields});
        lua::Table::readField(L, 4, "x", area.x);
        lua::Table::readField(L, 4, "y", area.y);
        lua::Table::readField(L, 4, "width", area.width);
        lua::Table::readField(L, 4, "height", area.height);
        lua::Table::readField(L, 4, "source", source);
    }
    lua::Userdata::emplace<Mesh>(L, std::make_shared<Mesh>(Mesh::grid(std::move(texture), columns, rows, area, source)));
    return 1;
}

std::size_t MeshLua::checkVertex(lua_State* L, int index, const Mesh& mesh) {
    const lua_Integer place = luaL_checkinteger(L, index);
    luaL_argcheck(L, place >= 1 && static_cast<std::size_t>(place) <= mesh.getVertices().size(), index, "the mesh has no such vertex");
    return static_cast<std::size_t>(place - 1);
}

// Moves vertices with `setPositions(buffer or list, first)`, x and y pairs from the vertex `first`, which defaults to 1.
int MeshLua::setPositions(lua_State* L) {
    Mesh& mesh = lua::Userdata::check<Mesh>(L, 1);
    const auto first = static_cast<std::size_t>(luaL_optinteger(L, 3, 1));
    luaL_argcheck(L, first >= 1, 3, "vertices count from 1");
    if (const core::FloatBuffer* buffer = lua::Userdata::test<core::FloatBuffer>(L, 2)) {
        mesh.setPositions(buffer->getValues(), first - 1);
        return 0;
    }
    std::vector<float> values;
    StrokeLua::readNumbers(L, 2, "positions", values);
    mesh.setPositions(values, first - 1);
    return 0;
}

// Copies positions with `readPositions(buffer, first)` into a float buffer, as many vertices as it holds pairs.
int MeshLua::readPositions(lua_State* L) {
    const Mesh& mesh = lua::Userdata::check<Mesh>(L, 1);
    core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 2);
    const auto first = static_cast<std::size_t>(luaL_optinteger(L, 3, 1));
    luaL_argcheck(L, first >= 1, 3, "vertices count from 1");
    mesh.readPositions(buffer.getValues(), first - 1);
    return 0;
}

int MeshLua::setVertex(lua_State* L) {
    Mesh& mesh = lua::Userdata::check<Mesh>(L, 1);
    const std::size_t place = checkVertex(L, 2, mesh);
    MeshVertex vertex = mesh.getVertices()[place];
    readVertex(L, 3, vertex);
    mesh.getVertices()[place] = vertex;
    return 0;
}

int MeshLua::vertexAt(lua_State* L) {
    const Mesh& mesh = lua::Userdata::check<Mesh>(L, 1);
    const MeshVertex& found = mesh.getVertices()[checkVertex(L, 2, mesh)];
    lua_createtable(L, 0, 5);
    lua::Stack::push(L, found.position.x);
    lua_setfield(L, -2, "x");
    lua::Stack::push(L, found.position.y);
    lua_setfield(L, -2, "y");
    lua::Stack::push(L, found.uv.x);
    lua_setfield(L, -2, "u");
    lua::Stack::push(L, found.uv.y);
    lua_setfield(L, -2, "v");
    lua::Stack::push(L, found.color);
    lua_setfield(L, -2, "color");
    return 1;
}

int MeshLua::setIndices(lua_State* L) {
    Mesh& mesh = lua::Userdata::check<Mesh>(L, 1);
    std::vector<std::uint32_t> indices;
    readIndices(L, 2, indices);
    mesh.setIndices(std::move(indices));
    return 0;
}

// Draws with `draw(order, x, y, rotation, scaleX, scaleY)`, which place the vertices without changing them.
int MeshLua::draw(lua_State* L) {
    const Mesh& mesh = lua::Userdata::check<Mesh>(L, 1);
    const DrawOrder order = lua::TypeConverter::readDrawOrder(L, 2);
    const math::Vec2 position{static_cast<float>(luaL_optnumber(L, 3, 0.0)), static_cast<float>(luaL_optnumber(L, 4, 0.0))};
    const auto rotation = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    const math::Vec2 scale{static_cast<float>(luaL_optnumber(L, 6, 1.0)), static_cast<float>(luaL_optnumber(L, 7, 1.0))};
    lua::Runtime::getEngine(L).getRenderer2D().drawMesh(mesh, order, math::Transform2D::compose(position, rotation, scale));
    return 0;
}

int MeshLua::vertexCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Mesh>(L, 1).getVertices().size());
    return 1;
}

int MeshLua::indexCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Mesh>(L, 1).getIndices().size());
    return 1;
}

int MeshLua::bounds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Mesh>(L, 1).getBounds());
    return 1;
}

int MeshLua::getTexture(lua_State* L) {
    const graphics::Texture& texture = lua::Userdata::check<Mesh>(L, 1).getTexture();
    if (!texture.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, texture);
    return 1;
}

int MeshLua::setTexture(lua_State* L) {
    lua::Userdata::check<Mesh>(L, 1).setTexture(lua_isnil(L, 3) ? graphics::Texture{} : lua::Stack::read<graphics::Texture>(L, 3));
    return 0;
}

void MeshLua::install(lua_State* L) {
    lua::ClassBuilder<Mesh>(L).function("setPositions", &lua::Binding::native<&setPositions>).function("readPositions", &lua::Binding::native<&readPositions>).function("setVertex", &lua::Binding::native<&setVertex>).function("vertex", &lua::Binding::native<&vertexAt>).function("setIndices", &lua::Binding::native<&setIndices>).function("draw", &lua::Binding::native<&draw>).property("vertexCount", &vertexCount).property("indexCount", &indexCount).property("bounds", &bounds).property("texture", &getTexture, &lua::Binding::native<&setTexture>).install();
}

} // namespace haylen::graphics2d
