#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/3d/animation/Pose3D.hpp"
#include "haylen/3d/animation/PoseSampler3D.hpp"
#include "haylen/3d/animation/Skeleton3D.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua::animation3d {

// A pose that Lua holds, with the sampler of the clip it sampled last and its cursors, so sampling one clip frame after frame binds it once.
struct ScriptedPose3D {
    haylen::animation3d::Pose3D pose;
    std::shared_ptr<const haylen::animation3d::PoseSampler3D> sampler;
    std::vector<std::size_t> cursors;
};

} // namespace haylen::lua::animation3d

namespace haylen::lua {

template <> struct Type<haylen::animation3d::Skeleton3D> {
    static constexpr const char* name = "haylen.Skeleton3D";
    using Storage = std::shared_ptr<haylen::animation3d::Skeleton3D>;
};

template <> struct Type<lua::animation3d::ScriptedPose3D> {
    static constexpr const char* name = "haylen.Pose3D";
    using Storage = std::shared_ptr<lua::animation3d::ScriptedPose3D>;
};

} // namespace haylen::lua

namespace haylen::lua::animation3d {

// Installs the `Skeleton3D` and `Pose3D` classes of `haylen.animation3d`: the bones of a character and the pose of one character, which clips write and skins read.
class Skeleton3DBinding final {
  public:
    static void install(lua_State* L);

    // Creates a skeleton with `newSkeleton(bones)`, a skeleton of a skin of a glTF document with `skeletonFromGltf(document, skin)` and a pose with `newPose(skeleton)`.
    static int create(lua_State* L);
    static int fromGltf(lua_State* L);
    static int createPose(lua_State* L);

    [[nodiscard]] static std::shared_ptr<const haylen::animation3d::Skeleton3D> checkSkeleton(lua_State* L, int index);
    [[nodiscard]] static ScriptedPose3D& checkPose(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 5> kBoneFields{"name", "parent", "position", "rotation", "scale"};

    // Reads a bone of the skeleton by its name or by its place counted from one.
    [[nodiscard]] static std::size_t readBone(lua_State* L, int index, const haylen::animation3d::Skeleton3D& skeleton);
    static void push(lua_State* L, std::shared_ptr<const haylen::animation3d::Skeleton3D> skeleton);

    static int boneCount(lua_State* L);
    static int bones(lua_State* L);
    static int findBone(lua_State* L);
    static int poseSkeleton(lua_State* L);
    static int setRest(lua_State* L);
    static int sample(lua_State* L);
    static int transform(lua_State* L);
    static int setTransform(lua_State* L);
    static int computeModelMatrices(lua_State* L);
    static int modelMatrix(lua_State* L);
};

} // namespace haylen::lua::animation3d
