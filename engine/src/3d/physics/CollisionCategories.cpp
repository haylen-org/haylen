#include "haylen/3d/physics/CollisionCategories.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace haylen::physics3d {

void CollisionCategories::readSection(const core::Json& section) {
    const auto found = section.find(kKey);
    if (found == section.end()) {
        return;
    }

    const core::Json& list = *found;
    if (!list.is_array() || list.size() > kMaxCategories) {
        throw std::invalid_argument("The value of \"physics3d.categories\" in \"haylen.json\" must be a list of up to 32 names, such as [\"ground\", \"player\"].");
    }

    std::vector<std::string> read;
    read.reserve(list.size());
    for (std::size_t index = 0; index < list.size(); ++index) {
        const core::Json& entry = list[index];
        if (!entry.is_string() || entry.get_ref<const std::string&>().empty()) {
            throw std::invalid_argument(std::format("Item {} of \"physics3d.categories\" in \"haylen.json\" must be a name that is not empty.", index + 1));
        }

        const std::string& name = entry.get_ref<const std::string&>();
        if (std::ranges::find(read, name) != read.end()) {
            throw std::invalid_argument(std::format("The category \"{}\" appears twice in \"physics3d.categories\" in \"haylen.json\".", name));
        }

        read.push_back(name);
    }

    names = std::move(read);
}

std::uint32_t CollisionCategories::getBit(std::string_view name) const {
    const auto found = std::ranges::find(names, name);
    if (found != names.end()) {
        return std::uint32_t{1} << static_cast<std::uint32_t>(found - names.begin());
    }

    if (names.empty()) {
        throw std::invalid_argument(std::format("No 3D collision category is named \"{}\". Name the categories of the app in \"physics3d.categories\" in \"haylen.json\".", name));
    }

    std::string known;
    for (const std::string& each : names) {
        known += known.empty() ? std::format("\"{}\"", each) : std::format(", \"{}\"", each);
    }

    throw std::invalid_argument(std::format("No 3D collision category is named \"{}\". The categories of \"physics3d.categories\" in \"haylen.json\" are {}.", name, known));
}

} // namespace haylen::physics3d
