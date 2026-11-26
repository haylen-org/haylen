#include "3d/animation/MorphTargets3DBinding.hpp"

#include <lua.hpp>

#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include "3d/animation/VertexBuffers3D.hpp"
#include "3d/gltf/GltfBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::animation3d {

using haylen::animation3d::FaceExpression3D;
using haylen::animation3d::MorphTargets3D;

const MorphTargets3D& MorphTargets3DBinding::check(lua_State* L) {
    return lua::Userdata::check<MorphTargets3D>(L, 1);
}

void MorphTargets3DBinding::readVectors(lua_State* L, int index, std::string_view what, std::vector<haylen::math::Vec3>& target) {
    lua::core::FloatBufferBinding::readNumbers(L, index, what, numbers);
    if (numbers.size() % 3 != 0) {
        throw std::invalid_argument(std::format("The {} hold {} numbers, while they take three for each vertex.", what, numbers.size()));
    }

    VertexBuffers3D::read(numbers, numbers.size() / 3, target, what);
}

// Each target is `{name, vertices, positions, normals, tangents}`, with its vertices counted from 1 and three numbers of each attribute for each of them.
int MorphTargets3DBinding::create(lua_State* L) {
    const auto vertexCount = lua::Stack::read<std::size_t>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<MorphTargets3D::Target> targets;
    const lua_Integer count = luaL_len(L, 2);
    for (lua_Integer place = 1; place <= count; ++place) {
        lua_rawgeti(L, 2, place);
        lua::Table::requireItemType(L, "targets", place, LUA_TTABLE);
        const int entry = lua_gettop(L);
        lua::Table::checkFields(L, entry, {kTargetFields});
        MorphTargets3D::Target target;
        lua::Table::readField(L, entry, "name", target.name);
        lua_getfield(L, entry, "vertices");
        lua::core::FloatBufferBinding::readNumbers(L, lua_gettop(L), "vertices", numbers);
        lua_pop(L, 1);
        for (const float vertex : numbers) {
            if (!(vertex >= 1.0F && vertex < 4294967296.0F) || vertex != std::floor(vertex)) {
                throw std::invalid_argument(std::format("The vertices of morph target {} are whole numbers counted from 1, while one of them is {}.", place, vertex));
            }

            target.vertices.push_back(static_cast<std::uint32_t>(vertex) - 1);
        }

        for (const auto& [key, list] : {std::pair{"positions", &target.positions}, std::pair{"normals", &target.normals}, std::pair{"tangents", &target.tangents}}) {
            lua_getfield(L, entry, key);
            if (!lua_isnil(L, -1)) {
                readVectors(L, lua_gettop(L), key, *list);
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 1);
        targets.push_back(std::move(target));
    }

    lua::Userdata::emplace<MorphTargets3D>(L, std::make_shared<MorphTargets3D>(vertexCount, std::move(targets)));
    return 1;
}

int MorphTargets3DBinding::fromGltf(lua_State* L) {
    const haylen::gltf::Document& document = lua::Userdata::check<haylen::gltf::Document>(L, 1);
    const std::size_t mesh = lua::gltf::GltfBinding::checkObject(L, 2, "meshes", document.meshes.size());
    lua_Integer primitive = 1;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kGltfFields});
        lua::Table::readField(L, 3, "primitive", primitive);
    }

    const std::size_t primitives = document.meshes[mesh].primitives.size();
    if (primitive < 1 || static_cast<std::size_t>(primitive) > primitives) {
        throw std::invalid_argument(std::format("The mesh has {} primitives, counted from 1, so it has no primitive {}.", primitives, primitive));
    }

    lua::Userdata::emplace<MorphTargets3D>(L, std::make_shared<MorphTargets3D>(MorphTargets3D::fromGltf(document, mesh, static_cast<std::size_t>(primitive - 1))));
    return 1;
}

void MorphTargets3DBinding::pushFaceExpressions(lua_State* L) {
    const auto& expressions = haylen::core::EnumNames<FaceExpression3D>::kNames;
    lua_createtable(L, static_cast<int>(expressions.size()), 0);
    for (std::size_t index = 0; index < expressions.size(); ++index) {
        lua::Stack::push(L, expressions[index].first);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

int MorphTargets3DBinding::targetCount(lua_State* L) {
    lua::Stack::push(L, check(L).getTargetCount());
    return 1;
}

int MorphTargets3DBinding::vertexCount(lua_State* L) {
    lua::Stack::push(L, check(L).getVertexCount());
    return 1;
}

int MorphTargets3DBinding::memorySize(lua_State* L) {
    lua::Stack::push(L, check(L).getMemorySize());
    return 1;
}

int MorphTargets3DBinding::names(lua_State* L) {
    const std::span<const MorphTargets3D::Target> targets = check(L).getTargets();
    lua_createtable(L, static_cast<int>(targets.size()), 0);
    for (std::size_t index = 0; index < targets.size(); ++index) {
        lua::Stack::push(L, targets[index].name);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Returns the place of the target of a name, counted from 1, or nil, such as the target of a face expression by its name.
int MorphTargets3DBinding::find(lua_State* L) {
    const std::optional<std::size_t> target = check(L).findTarget(lua::Stack::read<std::string_view>(L, 2));
    if (!target) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, *target + 1);
    return 1;
}

std::span<float> MorphTargets3DBinding::readTarget(lua_State* L, int options, const char* key, std::size_t vertices) {
    lua_getfield(L, options, key);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return {};
    }

    const std::span<float> target = lua::Userdata::check<haylen::core::FloatBuffer>(L, -1).getValues();
    lua_pop(L, 1);
    VertexBuffers3D::requireRoom(target, vertices, key);
    return target;
}

// Morphs vertices with `apply(weights, positions, morphedPositions[, {normals, morphedNormals, tangents, morphedTangents}])`, with one weight for each target in a list or a float buffer and three floats for each vertex in float buffers.
int MorphTargets3DBinding::apply(lua_State* L) {
    const MorphTargets3D& targets = check(L);
    const std::size_t vertices = targets.getVertexCount();
    lua::core::FloatBufferBinding::readNumbers(L, 2, "weights", weights);
    VertexBuffers3D::read(lua::Userdata::check<haylen::core::FloatBuffer>(L, 3).getValues(), vertices, positions, "positions");
    const std::span<float> positionTarget = lua::Userdata::check<haylen::core::FloatBuffer>(L, 4).getValues();
    VertexBuffers3D::requireRoom(positionTarget, vertices, "morphed positions");
    normals.clear();
    tangents.clear();
    std::span<float> normalTarget;
    std::span<float> tangentTarget;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kApplyFields});
        const std::span<float> normalSource = readTarget(L, 5, "normals", vertices);
        const std::span<float> tangentSource = readTarget(L, 5, "tangents", vertices);
        normalTarget = readTarget(L, 5, "morphedNormals", vertices);
        tangentTarget = readTarget(L, 5, "morphedTangents", vertices);
        if (normalSource.empty() != normalTarget.empty() || tangentSource.empty() != tangentTarget.empty()) {
            throw std::invalid_argument("The options \"normals\" and \"morphedNormals\" come together, and so do \"tangents\" and \"morphedTangents\".");
        }

        if (!normalSource.empty()) {
            VertexBuffers3D::read(normalSource, vertices, normals, "normals");
        }

        if (!tangentSource.empty()) {
            VertexBuffers3D::read(tangentSource, vertices, tangents, "tangents");
        }
    }

    morphedPositions.resize(vertices);
    morphedNormals.resize(normals.size());
    morphedTangents.resize(tangents.size());
    targets.apply(weights, {.positions = positions, .normals = normals, .tangents = tangents, .morphedPositions = morphedPositions, .morphedNormals = morphedNormals, .morphedTangents = morphedTangents});
    VertexBuffers3D::write(morphedPositions, positionTarget);
    VertexBuffers3D::write(morphedNormals, normalTarget);
    VertexBuffers3D::write(morphedTangents, tangentTarget);
    return 0;
}

void MorphTargets3DBinding::install(lua_State* L) {
    lua::ClassBuilder<MorphTargets3D>(L).property("targetCount", &lua::Binding::native<&targetCount>).property("vertexCount", &lua::Binding::native<&vertexCount>).property("memorySize", &lua::Binding::native<&memorySize>).property("names", &lua::Binding::native<&names>).function("find", &lua::Binding::native<&find>).function("apply", &lua::Binding::native<&apply>).install();
}

} // namespace haylen::lua::animation3d
