#include "platform/android/AndroidAssetPackage.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::platform {

AndroidAssetPackage::AndroidAssetPackage(AAssetManager* manager, std::string folder, std::vector<std::string> folderFiles) : assets(manager), root(std::move(folder)), files(std::move(folderFiles)) {}

std::shared_ptr<AndroidAssetPackage> AndroidAssetPackage::openIndexed(AAssetManager* manager, std::string folder) {
    AndroidAssetReader::Handle index(AAssetManager_open(manager, (folder + "/haylen-package-index.json").c_str(), AASSET_MODE_BUFFER), &AAsset_close);
    if (index == nullptr) {
        throw std::runtime_error("The APK has no app package at \"" + folder + "\", or its \"haylen-package-index.json\" is missing.");
    }
    AndroidAssetReader reader(std::move(index));
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(reader.getSize()));
    reader.readExactly(0, bytes);
    return std::make_shared<AndroidAssetPackage>(manager, std::move(folder), core::Json::parse(bytes.begin(), bytes.end()).get<std::vector<std::string>>());
}

std::shared_ptr<AndroidAssetPackage> AndroidAssetPackage::openFlat(AAssetManager* manager, std::string folder) {
    const std::unique_ptr<AAssetDir, decltype(&AAssetDir_close)> directory(AAssetManager_openDir(manager, folder.c_str()), &AAssetDir_close);
    std::vector<std::string> names;
    for (const char* name = AAssetDir_getNextFileName(directory.get()); name != nullptr; name = AAssetDir_getNextFileName(directory.get())) {
        names.emplace_back(name);
    }
    std::ranges::sort(names);
    return std::make_shared<AndroidAssetPackage>(manager, std::move(folder), std::move(names));
}

bool AndroidAssetPackage::exists(std::string_view path) const {
    return openAsset(root + "/" + io::Path::normalize(path)) != nullptr;
}

std::uint64_t AndroidAssetPackage::getFileSize(std::string_view path) const {
    return static_cast<std::uint64_t>(AAsset_getLength64(requireAsset(path).get()));
}

std::unique_ptr<io::PackageReader> AndroidAssetPackage::openReader(std::string_view path) const {
    return std::make_unique<AndroidAssetReader>(requireAsset(path));
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

AndroidAssetReader::Handle AndroidAssetPackage::openAsset(const std::string& path) const {
    return {AAssetManager_open(assets, path.c_str(), AASSET_MODE_RANDOM), &AAsset_close};
}

AndroidAssetReader::Handle AndroidAssetPackage::requireAsset(std::string_view path) const {
    const std::string entry = io::Path::normalize(path);
    AndroidAssetReader::Handle asset = openAsset(root + "/" + entry);
    if (asset == nullptr) {
        throw std::runtime_error("The package file \"" + root + "/" + entry + "\" was not found.");
    }
    return asset;
}

} // namespace haylen::platform
