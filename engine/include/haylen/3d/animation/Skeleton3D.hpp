#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Mat4.hpp"
#include "haylen/math/Transform3D.hpp"

namespace haylen::gltf {
struct Document;
}

namespace haylen::animation3d {

// The hierarchy of the bones of a character: each bone has a name, a parent that comes before it and a rest pose relative to that parent. A skeleton is shared data that never changes once made, and each character keeps its own `Pose3D` of it.
class Skeleton3D final {
  public:
    struct Bone {
        std::string name{};
        std::optional<std::size_t> parent{};
        math::Transform3D rest{};
    };

    // The most bones of a skeleton, which the 16-bit indices of the joints of vertices reach.
    static constexpr std::size_t kMaxBones = 65536;

    // Throws `std::invalid_argument` for no bones, more than `kMaxBones`, a name that is empty or taken, a parent that does not come before its bone, and a rest pose that is not finite.
    explicit Skeleton3D(std::vector<Bone> bones);

    // Makes the skeleton of a skin of a glTF document: every joint of the skin with every node above it up to the root of its hierarchy, so the nodes between joints and above them place the joints as the file does. Each bone takes the name of its node by the rule of `Clip3D::fromGltf`, so the clips of the file move it, and the rest pose of its node. Throws `std::out_of_range` for a skin the document lacks and `std::invalid_argument` with a sentence that names the file for a skin that does not make a skeleton.
    [[nodiscard]] static Skeleton3D fromGltf(const gltf::Document& document, std::size_t skin);

    [[nodiscard]] std::span<const Bone> getBones() const noexcept {
        return bones;
    }
    [[nodiscard]] std::size_t getBoneCount() const noexcept {
        return bones.size();
    }
    [[nodiscard]] std::optional<std::size_t> findBone(std::string_view name) const noexcept;

    // Writes the matrix of each bone in the space of the skeleton, its parent's matrix times its local transform, for the local transforms of a pose. Allocates nothing. Throws `std::invalid_argument` for spans of other sizes than the bones.
    void computeModelMatrices(std::span<const math::Transform3D> local, std::span<math::Mat4> result) const;

  private:
    std::vector<Bone> bones;
    std::map<std::string, std::size_t, std::less<>> indices;
};

} // namespace haylen::animation3d
