#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "haylen/3d/animation/Clip3D.hpp"
#include "haylen/3d/animation/Skeleton3D.hpp"
#include "haylen/math/Transform3D.hpp"

namespace haylen::animation3d {

// A clip bound to a skeleton once: each track that moves the `position`, `rotation` or `scale` of a bone of the skeleton by its name writes into that bone of a pose, while the other tracks belong to other parts of the engine. Many characters share one sampler.
class PoseSampler3D final {
  public:
    // Throws `std::invalid_argument` for no clip or skeleton, and for a track of a bone whose type is not a `vec3` for a position or a scale or a `quat` for a rotation.
    PoseSampler3D(std::shared_ptr<const Clip3D> clip, std::shared_ptr<const Skeleton3D> skeleton);

    [[nodiscard]] const Clip3D& getClip() const noexcept {
        return *clip;
    }
    [[nodiscard]] const std::shared_ptr<const Clip3D>& getClipPointer() const noexcept {
        return clip;
    }
    [[nodiscard]] const Skeleton3D& getSkeleton() const noexcept {
        return *skeleton;
    }

    // The tracks that move bones, which take one cursor each.
    [[nodiscard]] std::size_t getBoundTrackCount() const noexcept {
        return bindings.size();
    }

    // Writes the bound tracks at a time into the local transforms of a pose of the skeleton, leaving the bones that no track moves as they are. Allocates nothing. Throws `std::invalid_argument` for spans of other sizes than the bones and the bound tracks.
    void sample(float time, std::span<math::Transform3D> local, std::span<std::size_t> cursors) const;

  private:
    enum class Part : std::uint8_t {
        Position,
        Rotation,
        Scale,
    };

    struct Binding {
        std::size_t track = 0;
        std::size_t bone = 0;
        Part part = Part::Position;
    };

    std::shared_ptr<const Clip3D> clip;
    std::shared_ptr<const Skeleton3D> skeleton;
    std::vector<Binding> bindings;
};

} // namespace haylen::animation3d
