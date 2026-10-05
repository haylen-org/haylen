#include "io/OverlayPackage.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/io/Path.hpp"
#include "io/MemoryReader.hpp"

namespace haylen::io {

OverlayPackage::OverlayPackage(std::shared_ptr<const Package> basePackage) : base(std::move(basePackage)) {
    if (!base) {
        throw std::invalid_argument("An overlay package needs a base package.");
    }
}

void OverlayPackage::setFile(std::string_view path, std::vector<std::uint8_t> bytes) {
    std::string entry = Path::normalize(path);
    Bytes shared = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
    const std::scoped_lock lock(mutex);
    files.insert_or_assign(std::move(entry), std::move(shared));
}

bool OverlayPackage::removeFile(std::string_view path) {
    const bool existed = exists(path);
    const std::scoped_lock lock(mutex);
    files.insert_or_assign(Path::normalize(path), nullptr);
    return existed;
}

std::optional<OverlayPackage::Bytes> OverlayPackage::findOwn(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    const std::scoped_lock lock(mutex);
    const auto found = files.find(entry);
    return found != files.end() ? std::optional(found->second) : std::nullopt;
}

bool OverlayPackage::exists(std::string_view path) const {
    const std::optional<Bytes> own = findOwn(path);
    return own ? *own != nullptr : base->exists(path);
}

std::uint64_t OverlayPackage::getFileSize(std::string_view path) const {
    return findOwn(path) ? openReader(path)->getSize() : base->getFileSize(path);
}

std::unique_ptr<PackageReader> OverlayPackage::openReader(std::string_view path) const {
    const std::optional<Bytes> own = findOwn(path);
    if (!own) {
        return base->openReader(path);
    }
    if (*own == nullptr) {
        throw std::runtime_error("The package file \"" + std::string(getName()) + "/" + Path::normalize(path) + "\" was not found.");
    }
    return std::make_unique<MemoryReader>(*own);
}

bool OverlayPackage::isLuaBytecode(std::string_view path) const {
    return !findOwn(path) && base->isLuaBytecode(path);
}

std::vector<std::string> OverlayPackage::list(std::string_view directory) const {
    std::vector<std::string> listed = base->list(directory);
    const std::string prefix = Path::normalize(directory);
    const std::scoped_lock lock(mutex);
    for (const auto& [path, bytes] : files) {
        if (!Path::isInside(path, prefix)) {
            continue;
        }
        if (bytes == nullptr) {
            std::erase(listed, path);
        } else if (std::ranges::find(listed, path) == listed.end()) {
            listed.push_back(path);
        }
    }
    std::ranges::sort(listed);
    return listed;
}

} // namespace haylen::io
