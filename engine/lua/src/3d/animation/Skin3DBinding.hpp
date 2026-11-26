#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/3d/animation/Skin3D.hpp"
#include "haylen/3d/animation/SkinWeights3D.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/DualQuat.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Vec3.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::animation3d::Skin3D> {
    static constexpr const char* name = "haylen.Skin3D";
    using Storage = std::shared_ptr<haylen::animation3d::Skin3D>;
};

template <> struct Type<haylen::animation3d::SkinWeights3D> {
    static constexpr const char* name = "haylen.SkinWeights3D";
    using Storage = std::shared_ptr<haylen::animation3d::SkinWeights3D>;
};

} // namespace haylen::lua

namespace haylen::lua::animation3d {

// Installs the `Skin3D` and `SkinWeights3D` classes of `haylen.animation3d`: the joints of a skeleton that bend a mesh with their palettes in float buffers, and the joints and weights of its vertices, which bend positions and normals on the CPU.
class Skin3DBinding final {
  public:
    static void install(lua_State* L);

    // Creates a skin with `newSkin(skeleton, joints)`, the skin of a glTF document with `skinFromGltf(document, skin, skeleton)` and the weights of a mesh of a glTF document with `skinWeightsFromGltf(document, mesh, skin[, {primitive, influences}])`.
    static int create(lua_State* L);
    static int fromGltf(lua_State* L);
    static int weightsFromGltf(lua_State* L);

    [[nodiscard]] static std::shared_ptr<const haylen::animation3d::Skin3D> checkSkin(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 2> kJointFields{"bone", "inverseBindMatrix"};
    static constexpr std::array<std::string_view, 2> kWeightsFields{"primitive", "influences"};
    static constexpr std::array<std::string_view, 3> kSkinFields{"normals", "skinnedNormals", "dualQuaternions"};
    static constexpr std::size_t kDualQuatFloats = 8;

    // The matrices, dual quaternions and vertices of calls from Lua, which every call reuses.
    inline static std::vector<haylen::math::Mat4> matrices;
    inline static std::vector<haylen::math::DualQuat> dualQuats;
    inline static std::vector<haylen::math::Vec3> positions;
    inline static std::vector<haylen::math::Vec3> normals;
    inline static std::vector<haylen::math::Vec3> skinnedPositions;
    inline static std::vector<haylen::math::Vec3> skinnedNormals;

    static void readVectors(std::span<const float> numbers, std::size_t count, std::vector<haylen::math::Vec3>& target, std::string_view what);
    static void writeVectors(const std::vector<haylen::math::Vec3>& source, std::span<float> numbers);

    static int jointCount(lua_State* L);
    static int joints(lua_State* L);
    static int skinSkeleton(lua_State* L);
    static int palette(lua_State* L);
    static int dualQuatPalette(lua_State* L);
    static int influenceCount(lua_State* L);
    static int vertexCount(lua_State* L);
    static int weightsJointCount(lua_State* L);
    static int readJoints(lua_State* L);
    static int readWeights(lua_State* L);
    static int skin(lua_State* L);
};

} // namespace haylen::lua::animation3d
