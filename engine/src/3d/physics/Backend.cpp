#include "haylen/3d/physics/Backend.hpp"

namespace haylen::physics3d {

std::shared_ptr<const Shape> Backend::createShape(const Shape::Settings& settings) const {
    settings.validate(getName());
    return buildShape(settings);
}

} // namespace haylen::physics3d
