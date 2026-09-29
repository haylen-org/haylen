#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::tiled {

// A Tiled world file, which places maps side by side in one coordinate space.
struct World {
    struct Placement {
        std::string path;
        math::Rect bounds{};
    };

    // Parses a world stored at file inside the package content folder. Maps listed by pattern are matched against the given package files.
    [[nodiscard]] static World parse(const core::Json& document, std::string_view file, const std::vector<std::string>& files);

    std::vector<Placement> maps;
};

} // namespace haylen::tiled
