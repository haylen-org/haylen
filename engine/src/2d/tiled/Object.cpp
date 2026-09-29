#include "haylen/2d/tiled/Object.hpp"

namespace haylen::tiled {

std::string_view Object::shapeName(Shape value) noexcept {
    switch (value) {
    case Shape::Rectangle:
        return "rectangle";
    case Shape::Ellipse:
        return "ellipse";
    case Shape::Capsule:
        return "capsule";
    case Shape::Point:
        return "point";
    case Shape::Polygon:
        return "polygon";
    case Shape::Polyline:
        return "polyline";
    case Shape::Text:
        return "text";
    case Shape::Tile:
        return "tile";
    }
    return "rectangle";
}

} // namespace haylen::tiled
