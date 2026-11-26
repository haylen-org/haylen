#pragma once

#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/3d/animation/MorphTargets3D.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/Vec3.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::animation3d::MorphTargets3D> {
    static constexpr const char* name = "haylen.MorphTargets3D";
    using Storage = std::shared_ptr<haylen::animation3d::MorphTargets3D>;
};

} // namespace haylen::lua

namespace haylen::lua::animation3d {

// Installs the `MorphTargets3D` class of `haylen.animation3d`, the morph targets of a mesh found by name and applied to its vertices in float buffers, and pushes the names of the face expressions.
class MorphTargets3DBinding final {
  public:
    static void install(lua_State* L);

    // Creates morph targets with `newMorphTargets(vertexCount, targets)` and reads those of a glTF document with `morphTargetsFromGltf(document, mesh[, {primitive}])`.
    static int create(lua_State* L);
    static int fromGltf(lua_State* L);

    // Pushes the list of the names of the face expressions, which the module holds in `faceExpressions`.
    static void pushFaceExpressions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 5> kTargetFields{"name", "vertices", "positions", "normals", "tangents"};
    static constexpr std::array<std::string_view, 1> kGltfFields{"primitive"};
    static constexpr std::array<std::string_view, 4> kApplyFields{"normals", "morphedNormals", "tangents", "morphedTangents"};

    // The numbers and vectors of calls from Lua, which every call reuses.
    inline static std::vector<float> numbers;
    inline static std::vector<float> weights;
    inline static std::vector<haylen::math::Vec3> positions;
    inline static std::vector<haylen::math::Vec3> normals;
    inline static std::vector<haylen::math::Vec3> tangents;
    inline static std::vector<haylen::math::Vec3> morphedPositions;
    inline static std::vector<haylen::math::Vec3> morphedNormals;
    inline static std::vector<haylen::math::Vec3> morphedTangents;

    [[nodiscard]] static const haylen::animation3d::MorphTargets3D& check(lua_State* L);

    // Reads the vectors of a list of numbers or a float buffer, three numbers each.
    static void readVectors(lua_State* L, int index, std::string_view what, std::vector<haylen::math::Vec3>& target);

    // Reads the float buffer of the option `key` of the table at `options`, which must hold three numbers for each vertex, or nothing when the option is absent.
    [[nodiscard]] static std::span<float> readTarget(lua_State* L, int options, const char* key, std::size_t vertices);

    static void writeVectors(const std::vector<haylen::math::Vec3>& source, std::span<float> target);

    static int targetCount(lua_State* L);
    static int vertexCount(lua_State* L);
    static int memorySize(lua_State* L);
    static int names(lua_State* L);
    static int find(lua_State* L);
    static int apply(lua_State* L);
};

} // namespace haylen::lua::animation3d
