#include "io/CompositePackage.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "haylen/io/Path.hpp"

namespace haylen::io {

CompositePackage::CompositePackage(std::string packageName, std::vector<std::shared_ptr<const Package>> packageLayers) : name(std::move(packageName)), layers(std::move(packageLayers)) {}

const Package& CompositePackage::find(std::string_view path) const {
    for (const std::shared_ptr<const Package>& layer : std::views::reverse(layers)) {
        if (layer->exists(path)) {
            return *layer;
        }
    }
    throw std::runtime_error("The package file \"" + name + "/" + Path::normalize(path) + "\" was not found.");
}

bool CompositePackage::exists(std::string_view path) const {
    return std::ranges::any_of(layers, [path](const std::shared_ptr<const Package>& layer) { return layer->exists(path); });
}

std::uint64_t CompositePackage::getFileSize(std::string_view path) const {
    return find(path).getFileSize(path);
}

std::unique_ptr<PackageReader> CompositePackage::openReader(std::string_view path) const {
    return find(path).openReader(path);
}

std::vector<std::string> CompositePackage::list(std::string_view directory) const {
    std::vector<std::string> files;
    for (const std::shared_ptr<const Package>& layer : layers) {
        std::vector<std::string> listed = layer->list(directory);
        files.insert(files.end(), std::make_move_iterator(listed.begin()), std::make_move_iterator(listed.end()));
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

} // namespace haylen::io
