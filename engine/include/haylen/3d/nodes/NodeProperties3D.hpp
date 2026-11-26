#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "haylen/3d/nodes/Inheritance3D.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::nodes3d {

// The local state of a new node. The transform is in meters and relative to the parent by the parts of `inheritance`. A hidden node hides its descendants, and a disabled node hides and stops them. The layers say which cameras and lights see the node, and the render order draws it before the nodes of higher orders.
struct NodeProperties3D {
    std::string name{};
    math::Vec3 position{};
    math::Quat rotation{};
    math::Vec3 scale{1.0F, 1.0F, 1.0F};
    bool visible = true;
    bool enabled = true;
    std::uint32_t layers = 1;

    // A node that never moves, which renderers may merge into batches and bakers may bake, so a move of it costs a rebuild of what took it in.
    bool isStatic = false;
    bool frustumCulling = true;
    int renderOrder = 0;
    Inheritance3D inheritance{};
    std::vector<std::string> tags{};

    // The data of the app, where the `extras` of a glTF node arrive.
    core::Json data{};
};

} // namespace haylen::nodes3d
