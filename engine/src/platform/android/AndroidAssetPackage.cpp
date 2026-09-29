#include "platform/android/AndroidAssetPackage.hpp"

#include <stdexcept>

#include "haylen/core/Json.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::platform {

AndroidAssetPackage::AndroidAssetPackage(AAssetManager* manager, std::string folder) : assets(manager), root(std::move(folder)) {
    const AssetHandle index = openAsset(root + "/haylen-package-index.json");
    if (index == nullptr) {
        throw std::runtime_error("The APK has no app package at " + root + ", or its haylen-package-index.json is missing.");
    }
    const std::vector<std::uint8_t> bytes = readAll(*index);
    files = core::Json::parse(bytes.begin(), bytes.end()).get<std::vector<std::string>>();
}

bool AndroidAssetPackage::exists(std::string_view path) const {
    return openAsset(root + "/" + io::Path::normalize(path)) != nullptr;
}

std::vector<std::uint8_t> AndroidAssetPackage::read(std::string_view path) const {
    const std::string entry = io::Path::normalize(path);
    const AssetHandle asset = openAsset(root + "/" + entry);
    if (asset == nullptr) {
        throw std::runtime_error("The package file '" + root + "/" + entry + "' was not found.");
    }
    return readAll(*asset);
}

std::vector<std::string> AndroidAssetPackage::list(std::string_view directory) const {
    const std::string prefix = io::Path::normalize(directory);
    std::vector<std::string> matches;
    for (const std::string& file : files) {
        if (io::Path::isInside(file, prefix)) {
            matches.push_back(file);
        }
    }
    return matches;
}

AndroidAssetPackage::AssetHandle AndroidAssetPackage::openAsset(const std::string& path) const {
    return AssetHandle(AAssetManager_open(assets, path.c_str(), AASSET_MODE_STREAMING));
}

std::vector<std::uint8_t> AndroidAssetPackage::readAll(AAsset& asset) {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(AAsset_getLength64(&asset)));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const int count = AAsset_read(&asset, bytes.data() + offset, bytes.size() - offset);
        if (count <= 0) {
            throw std::runtime_error("An APK asset could not be read to the end.");
        }
        offset += static_cast<std::size_t>(count);
    }
    return bytes;
}

} // namespace haylen::platform
