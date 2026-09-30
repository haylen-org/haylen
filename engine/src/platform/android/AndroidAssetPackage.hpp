#pragma once

#include <android/asset_manager.h>

#include <memory>
#include <string>
#include <vector>

#include "haylen/io/Package.hpp"

namespace haylen::platform {

// App package stored in the APK assets under a root folder. Android cannot list asset folders recursively, so the build writes the file list into `haylen-package-index.json` at the package root.
class AndroidAssetPackage final : public io::Package {
  public:
    AndroidAssetPackage(AAssetManager* manager, std::string folder);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return root;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;

  private:
    struct AssetCloser {
        void operator()(AAsset* asset) const noexcept {
            AAsset_close(asset);
        }
    };

    using AssetHandle = std::unique_ptr<AAsset, AssetCloser>;

    [[nodiscard]] AssetHandle openAsset(const std::string& path) const;
    [[nodiscard]] static std::vector<std::uint8_t> readAll(AAsset& asset);

    AAssetManager* assets;
    std::string root;
    std::vector<std::string> files;
};

} // namespace haylen::platform
