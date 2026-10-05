#pragma once

#include <android/asset_manager.h>

#include <memory>
#include <mutex>

#include "haylen/io/PackageReader.hpp"

namespace haylen::platform {

// Reads one asset of the APK in ranges.
class AndroidAssetReader final : public io::PackageReader {
  public:
    using Handle = std::unique_ptr<AAsset, decltype(&AAsset_close)>;

    explicit AndroidAssetReader(Handle openAsset);

    [[nodiscard]] std::uint64_t getSize() const noexcept override {
        return size;
    }
    [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override;

  private:
    Handle asset;
    std::uint64_t size;
    std::mutex mutex;
};

} // namespace haylen::platform
