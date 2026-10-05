#include "haylen/io/MemoryPackage.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/io/Path.hpp"
#include "io/MemoryReader.hpp"

namespace haylen::io {

MemoryPackage::MemoryPackage(std::string packageName, std::map<std::string, std::vector<std::uint8_t>> contents) : name(std::move(packageName)) {
    for (auto& [path, bytes] : contents) {
        files.insert_or_assign(Path::normalize(path), std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes)));
    }
}

void MemoryPackage::setFile(std::string_view path, std::vector<std::uint8_t> bytes) {
    std::string entry = Path::normalize(path);
    Bytes shared = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
    std::scoped_lock lock(mutex);
    files.insert_or_assign(std::move(entry), std::move(shared));
}

bool MemoryPackage::removeFile(std::string_view path) {
    const std::string entry = Path::normalize(path);
    std::scoped_lock lock(mutex);
    return files.erase(entry) > 0;
}

bool MemoryPackage::exists(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    std::scoped_lock lock(mutex);
    return files.contains(entry);
}

std::uint64_t MemoryPackage::getFileSize(std::string_view path) const {
    return find(path)->size();
}

std::unique_ptr<PackageReader> MemoryPackage::openReader(std::string_view path) const {
    return std::make_unique<MemoryReader>(find(path));
}

MemoryPackage::Bytes MemoryPackage::find(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    std::scoped_lock lock(mutex);
    const auto found = files.find(entry);
    if (found == files.end()) {
        throw std::runtime_error("The package file \"" + name + "/" + entry + "\" was not found.");
    }
    return found->second;
}

std::vector<std::string> MemoryPackage::list(std::string_view directory) const {
    const std::string prefix = Path::normalize(directory);
    std::vector<std::string> paths;

    std::scoped_lock lock(mutex);
    for (const auto& [path, bytes] : files) {
        if (Path::isInside(path, prefix)) {
            paths.push_back(path);
        }
    }
    return paths;
}

} // namespace haylen::io
