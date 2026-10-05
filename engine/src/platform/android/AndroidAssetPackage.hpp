#pragma once

#include <android/asset_manager.h>

#include <memory>
#include <string>
#include <vector>

#include "haylen/io/Package.hpp"
#include "platform/android/AndroidAssetReader.hpp"

namespace haylen::platform {

// App package stored in the APK assets under a root folder. Android cannot list asset folders recursively, so the build writes the file list into `haylen-package-index.json` at the package root.
class AndroidAssetPackage final : public io::Package {
  public:
    AndroidAssetPackage(AAssetManager* manager, std::string folder);

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
