#include "haylen/2d/tiled/WangSet.hpp"

#include <stdexcept>

namespace haylen::tiled {

std::uint8_t WangSet::readColor(const core::Json& value, std::string_view set, std::uint32_t tile) {
    if (!value.is_number_integer() || value.get<std::int64_t>() < 0 || value.get<std::int64_t>() > 255) {
        throw std::invalid_argument("The Wang set \"" + std::string(set) + "\" gives the tile " + std::to_string(tile) + " the color " + value.dump() + ", which is not a whole number from 0 to 255.");
    }

    return static_cast<std::uint8_t>(value.get<std::int64_t>());
}

} // namespace haylen::tiled
