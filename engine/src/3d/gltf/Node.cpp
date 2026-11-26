#include "haylen/3d/gltf/Node.hpp"

namespace haylen::gltf {

math::Mat4 Node::getLocalMatrix() const noexcept {
    if (matrix) {
        return *matrix;
    }

    return math::Mat4::compose(translation, rotation, scale);
}

} // namespace haylen::gltf
