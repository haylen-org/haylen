#pragma once

#include <lua.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Mat3.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Plane.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::lua::math {

// Installs the `Mat3` and `Mat4` classes of `haylen.math` with their constructors, the projections and views of cameras and the conversion of the range of depth.
class MatrixBinding final {
  public:
    static void install(lua_State* L);

    // Sets `mat3`, `mat4`, `compose`, `lookAt`, `perspective`, `asymmetricPerspective`, `orthographic` and `depthRangeConversion` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

    // Reads the optional table at `index` with the range and direction of depth that a projection was built with, `{depthRange = 'zeroToOne', reversedDepth = true}` by default.
    [[nodiscard]] static std::pair<haylen::math::Mat4::DepthRange, bool> readDepthOptions(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 6> kPerspectiveFields{"fieldOfView", "aspect", "nearPlane", "farPlane", "depthRange", "reversedDepth"};
    static constexpr std::array<std::string_view, 8> kBoundsFields{"left", "right", "bottom", "top", "nearPlane", "farPlane", "depthRange", "reversedDepth"};
    static constexpr std::array<std::string_view, 2> kDepthFields{"depthRange", "reversedDepth"};

    template <typename Matrix> static int equal(lua_State* L) {
        lua_pushboolean(L, lua::Stack::read<Matrix>(L, 1) == lua::Stack::read<Matrix>(L, 2) ? 1 : 0);
        return 1;
    }

    static int newMat3(lua_State* L);
    static int newMat4(lua_State* L);
    [[nodiscard]] static haylen::math::Mat4 compose(haylen::math::Vec3 translation, std::optional<haylen::math::Quat> rotation, std::optional<haylen::math::Vec3> scale);
    [[nodiscard]] static haylen::math::Mat4 lookAt(haylen::math::Vec3 eye, haylen::math::Vec3 target, std::optional<haylen::math::Vec3> up);
    static int perspective(lua_State* L);
    static int asymmetricPerspective(lua_State* L);
    static int orthographic(lua_State* L);

    // Reads the index of a row or a column, counted from 1 in Lua.
    [[nodiscard]] static std::size_t checkIndex(lua_State* L, int index, std::size_t count);
    static int mat3Get(lua_State* L);
    static int mat3Column(lua_State* L);
    static int mat3Row(lua_State* L);
    static int mat3Multiply(lua_State* L);
    static int mat3Unpack(lua_State* L);
    static int mat3ToString(lua_State* L);
    [[nodiscard]] static haylen::math::Mat3 mat3Inverse(const haylen::math::Mat3& self);
    [[nodiscard]] static haylen::math::Quat mat3Rotation(const haylen::math::Mat3& self);
    [[nodiscard]] static bool mat3Approximately(const haylen::math::Mat3& self, const haylen::math::Mat3& other, std::optional<float> epsilon);

    static int mat4Get(lua_State* L);
    static int mat4Column(lua_State* L);
    static int mat4Row(lua_State* L);
    static int mat4Multiply(lua_State* L);
    static int mat4Unpack(lua_State* L);
    static int mat4ToString(lua_State* L);
    static int mat4Decompose(lua_State* L);
    static int mat4ObliqueNearPlane(lua_State* L);
    [[nodiscard]] static haylen::math::Mat4 mat4Inverse(const haylen::math::Mat4& self);
    [[nodiscard]] static haylen::math::Mat4 mat4AffineInverse(const haylen::math::Mat4& self);
    [[nodiscard]] static haylen::math::Mat3 mat4NormalMatrix(const haylen::math::Mat4& self);
    [[nodiscard]] static bool mat4Approximately(const haylen::math::Mat4& self, const haylen::math::Mat4& other, std::optional<float> epsilon);
};

} // namespace haylen::lua::math
