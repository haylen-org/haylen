#pragma once

#include <string>

#include "haylen/2d/nodes/NodeContent.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::nodes2d {

// The local state of a new node. The transform is relative to the parent, the color multiplies the colors of the ancestors, a hidden node hides its descendants, and the layer adds to the layers of the ancestors, while the depth and the sort offset place the node itself.
struct NodeProperties {
    std::string name;
    math::Vec2 position{};
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};
    math::Color color = math::Color::white();
    bool visible = true;
    int layer = 0;
    float depth = 0.0F;
    float sortOffset = 0.0F;
    NodeContent content;
};

} // namespace haylen::nodes2d
