#pragma once

#include <android/asset_manager.h>

#include <memory>
#include <string>
#include <vector>

#include "haylen/io/Package.hpp"
#include "platform/android/AndroidAssetReader.hpp"

namespace haylen::platform {

// Files stored in the APK assets under a root folder: the app package of a development build, whose files `haylen-package-index.json` at the package root lists, since Android cannot list asset folders recursively, or the flat folder of a protected release, which Android lists itself.
class AndroidAssetPackage final : public io::Package {
  public:
    [[nodiscard]] static std::shared_ptr<AndroidAssetPackage> openIndexed(AAssetManager* manager, std::string folder);
    [[nodiscard]] static std::shared_ptr<AndroidAssetPackage> openFlat(AAssetManager* manager, std::string folder);

    AndroidAssetPackage(AAssetManager* manager, std::string folder, std::vector<std::string> folderFiles);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return root;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<io::PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;

  private:
    [[nodiscard]] AndroidAssetReader::Handle openAsset(const std::string& path) const;
    [[nodiscard]] AndroidAssetReader::Handle requireAsset(std::string_view path) const;

    AAssetManager* assets;
    std::string root;
    std::vector<std::string> files;
};

} // namespace haylen::platform
