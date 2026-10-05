#include "haylen/2d/graphics/SpriteEffect.hpp"

#include <cmath>
#include <stdexcept>

namespace haylen::graphics2d {

void SpriteEffect::validate() const {
    const bool shares = dissolve >= 0.0F && dissolve <= 1.0F && dissolveEdge >= 0.0F && dissolveEdge <= 1.0F;
    const bool sizes = dissolveSize >= 1.0F && dissolveSize <= kMaxReach && outlineWidth >= 0.0F && outlineWidth <= kMaxReach && glowSize >= 0.0F && glowSize <= kMaxReach;
    if (!shares || !sizes) {
        throw std::invalid_argument("A sprite effect needs a dissolve and a dissolve edge from 0 to 1, a dissolve size from 1 to 64 and an outline width and a glow size from 0 to 64.");
    }
}

} // namespace haylen::graphics2d
