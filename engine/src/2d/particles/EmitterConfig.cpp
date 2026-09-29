#include "haylen/2d/particles/EmitterConfig.hpp"

namespace haylen::particles2d {

const std::array<std::pair<std::string_view, EmitterConfig::Shape>, 5> EmitterConfig::kShapeNames{{{"point", Shape::Point}, {"circle", Shape::Circle}, {"ring", Shape::Ring}, {"rectangle", Shape::Rectangle}, {"cone", Shape::Cone}}};

std::optional<EmitterConfig::Shape> EmitterConfig::shapeFromName(std::string_view name) noexcept {
    for (const auto& [text, value] : kShapeNames) {
        if (text == name) {
            return value;
        }
    }
    return std::nullopt;
}

std::string_view EmitterConfig::shapeName(Shape value) noexcept {
    for (const auto& [text, candidate] : kShapeNames) {
        if (candidate == value) {
            return text;
        }
    }
    return kShapeNames.front().first;
}

} // namespace haylen::particles2d
