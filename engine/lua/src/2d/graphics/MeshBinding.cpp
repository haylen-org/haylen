#include "2d/graphics/MeshBinding.hpp"

#include <lua.hpp>

#include <optional>
#include <utility>

#include "2d/graphics/StrokeBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::graphics2d {

void MeshBinding::readVertex(lua_State* L, int index, haylen::graphics2d::MeshVertex& vertex) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kVertexFields});
    lua::Table::readField(L, index, "x", vertex.position.x);
    lua::Table::readField(L, index, "y", vertex.position.y);
    lua::Table::readField(L, index, "u", vertex.uv.x);
    lua::Table::readField(L, index, "v", vertex.uv.y);
    lua::Table::readField(L, index, "color", vertex.color);
}

void MeshBinding::readVertices(lua_State* L, int index, std::vector<haylen::graphics2d::MeshVertex>& vertices) {
    luaL_checktype(L, index, LUA_TTABLE);
    vertices.assign(lua_rawlen(L, index), haylen::graphics2d::MeshVertex{});
    for (std::size_t place = 0; place < vertices.size(); ++place) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(place + 1));
        readVertex(L, lua_gettop(L), vertices[place]);
        lua_pop(L, 1);
    }
}

void MeshBinding::readIndices(lua_State* L, int index, std::vector<std::uint32_t>& indices) {
    lua::Stack::readInto(L, index, indices);
    for (std::uint32_t& entry : indices) {
        luaL_argcheck(L, entry >= 1, index, "mesh indices start at 1");
        --entry;
    }
}

int MeshBinding::create(lua_State* L) {
    haylen::graphics::Texture texture = lua_isnil(L, 1) ? haylen::graphics::Texture{} : lua::Stack::read<haylen::graphics::Texture>(L, 1);
    std::vector<haylen::graphics2d::MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    readVertices(L, 2, vertices);
    readIndices(L, 3, indices);
    lua::Userdata::emplace<haylen::graphics2d::Mesh>(L, std::make_shared<haylen::graphics2d::Mesh>(std::move(texture), std::move(vertices), std::move(indices)));
    return 1;
}

int MeshBinding::createGrid(lua_State* L) {
    haylen::graphics::Texture texture = lua_isnil(L, 1) ? haylen::graphics::Texture{} : lua::Stack::read<haylen::graphics::Texture>(L, 1);
    const auto columns = lua::Stack::read<int>(L, 2);
    const auto rows = lua::Stack::read<int>(L, 3);
    haylen::math::Rect area{0.0F, 0.0F, texture.isValid() ? texture.getSize().x : 1.0F, texture.isValid() ? texture.getSize().y : 1.0F};
    std::optional<haylen::math::Rect> source;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kGridFields});
        lua::Table::readField(L, 4, "x", area.x);
        lua::Table::readField(L, 4, "y", area.y);
        lua::Table::readField(L, 4, "width", area.width);
        lua::Table::readField(L, 4, "height", area.height);
        lua::Table::readField(L, 4, "source", source);
    }

    lua::Userdata::emplace<haylen::graphics2d::Mesh>(L, std::make_shared<haylen::graphics2d::Mesh>(haylen::graphics2d::Mesh::grid(std::move(texture), columns, rows, area, source)));
    return 1;
}

std::size_t MeshBinding::checkVertex(lua_State* L, int index, const haylen::graphics2d::Mesh& mesh) {
    const lua_Integer place = luaL_checkinteger(L, index);
    luaL_argcheck(L, place >= 1 && static_cast<std::size_t>(place) <= mesh.getVertices().size(), index, "the mesh has no such vertex");
    return static_cast<std::size_t>(place - 1);
}

// The first vertex is counted from 1 and defaults to 1, and the index it returns counts from 0.
std::size_t MeshBinding::checkFirst(lua_State* L, int index) {
    const lua_Integer first = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, first >= 1, index, "vertices count from 1");
    return static_cast<std::size_t>(first - 1);
}

// Moves vertices with `setPositions(buffer or list, first)`, x and y pairs from the vertex `first`, which defaults to 1.
int MeshBinding::setPositions(lua_State* L) {
    haylen::graphics2d::Mesh& mesh = lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1);
    const std::size_t first = checkFirst(L, 3);

    if (const haylen::core::FloatBuffer* buffer = lua::Userdata::test<haylen::core::FloatBuffer>(L, 2)) {
        mesh.setPositions(buffer->getValues(), first);
        return 0;
    }

    StrokeBinding::readNumbers(L, 2, "positions", positionScratch);
    mesh.setPositions(positionScratch, first);
    return 0;
}

// Copies positions with `readPositions(buffer, first)` into a float buffer, as many vertices as it holds pairs.
int MeshBinding::readPositions(lua_State* L) {
    const haylen::graphics2d::Mesh& mesh = lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1);
    haylen::core::FloatBuffer& buffer = lua::Userdata::check<haylen::core::FloatBuffer>(L, 2);
    const std::size_t first = checkFirst(L, 3);
    mesh.readPositions(buffer.getValues(), first);
    return 0;
}

int MeshBinding::setVertex(lua_State* L) {
    haylen::graphics2d::Mesh& mesh = lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1);
    const std::size_t place = checkVertex(L, 2, mesh);
    haylen::graphics2d::MeshVertex vertex = mesh.getVertices()[place];
    readVertex(L, 3, vertex);
    mesh.getVertices()[place] = vertex;
    return 0;
}

int MeshBinding::vertexAt(lua_State* L) {
    const haylen::graphics2d::Mesh& mesh = lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1);
    const haylen::graphics2d::MeshVertex& found = mesh.getVertices()[checkVertex(L, 2, mesh)];
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

int MeshBinding::setIndices(lua_State* L) {
    haylen::graphics2d::Mesh& mesh = lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1);
    std::vector<std::uint32_t> indices;
    readIndices(L, 2, indices);
    mesh.setIndices(std::move(indices));
    return 0;
}

// Draws the mesh with `mesh:draw(x, y, options)`, whose options hold the rotation, the scale, whether the outline fades and the draw order, which place the vertices without changing them.
int MeshBinding::draw(lua_State* L) {
    const haylen::graphics2d::Mesh& mesh = lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1);
    const haylen::math::Vec2 position{lua::Converter<float>::readOr(L, 2, 0.0), lua::Converter<float>::readOr(L, 3, 0.0)};
    float rotation = 0.0F;
    haylen::math::Vec2 scale{1.0F, 1.0F};
    bool fadeEdges = true;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 4, kDrawFields, {lua::TypeConverter::kDrawOrderFields}, [L, &rotation, &scale, &fadeEdges](std::string_view key) {
            if (key == "rotation") {
                lua::Table::readValue(L, key, rotation);
            } else if (key == "scaleX") {
                lua::Table::readValue(L, key, scale.x);
            } else if (key == "scaleY") {
                lua::Table::readValue(L, key, scale.y);
            } else if (key == "fadeEdges") {
                lua::Table::readValue(L, key, fadeEdges);
            }
        });
        // clang-format on
    }

    lua::Runtime::getEngine(L).getRenderer2D().drawMesh(mesh, lua::TypeConverter::readDrawOrder(L, 4, {kDrawFields}), haylen::math::Transform2D::compose(position, rotation, scale), fadeEdges);
    return 0;
}

int MeshBinding::vertexCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1).getVertices().size());
    return 1;
}

int MeshBinding::indexCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1).getIndices().size());
    return 1;
}

int MeshBinding::bounds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1).getBounds());
    return 1;
}

int MeshBinding::getTexture(lua_State* L) {
    const haylen::graphics::Texture& texture = lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1).getTexture();
    if (!texture.isValid()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, texture);
    return 1;
}

int MeshBinding::setTexture(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Mesh>(L, 1).setTexture(lua_isnil(L, 3) ? haylen::graphics::Texture{} : lua::Stack::read<haylen::graphics::Texture>(L, 3));
    return 0;
}

void MeshBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::Mesh>(L).function("setPositions", &lua::Binding::native<&setPositions>).function("readPositions", &lua::Binding::native<&readPositions>).function("setVertex", &lua::Binding::native<&setVertex>).function("vertex", &lua::Binding::native<&vertexAt>).function("setIndices", &lua::Binding::native<&setIndices>).function("draw", &lua::Binding::native<&draw>).property("vertexCount", &vertexCount).property("indexCount", &indexCount).property("bounds", &bounds).property("texture", &getTexture, &lua::Binding::native<&setTexture>).install();
}

} // namespace haylen::lua::graphics2d
