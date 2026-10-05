#include "content/HpakPackage.hpp"

#include <stdexcept>
#include <utility>

#include "content/HpakFileReader.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

HpakPackage::HpakPackage(std::string packageName, std::shared_ptr<const Catalog> packageCatalog, std::shared_ptr<const ShardSet> packageShards) : name(std::move(packageName)), catalog(std::move(packageCatalog)), shards(std::move(packageShards)) {}

Catalog::File HpakPackage::find(std::string_view path) const {
    const std::string entry = io::Path::normalize(path);
    const std::optional<std::uint64_t> index = catalog->findFile(entry);
    if (!index) {
        throw std::runtime_error("The package file \"" + name + "/" + entry + "\" was not found.");
    }
    return catalog->getFile(*index);
}

bool HpakPackage::exists(std::string_view path) const {
    return catalog->findFile(io::Path::normalize(path)).has_value();
}

std::uint64_t HpakPackage::getFileSize(std::string_view path) const {
    return find(path).size;
}

std::unique_ptr<io::PackageReader> HpakPackage::openReader(std::string_view path) const {
    return std::make_unique<HpakFileReader>(catalog, shards, find(path));
}

std::vector<std::string> HpakPackage::list(std::string_view directory) const {
    const auto [first, last] = catalog->findFolder(io::Path::normalize(directory));
    std::vector<std::string> files;
    files.reserve(static_cast<std::size_t>(last - first));
    for (std::uint64_t index = first; index < last; ++index) {
        files.emplace_back(catalog->getFile(index).path);
    }
    return files;
}

} // namespace haylen::content
