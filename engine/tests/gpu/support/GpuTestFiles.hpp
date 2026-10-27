#pragma once

#include <filesystem>

namespace haylen::test {

// Finds the folders of the GPU tests: the reference images, and the folder that keeps the images of the scenes that differ. A build for a machine finds them in the source and build trees, the test app of an Apple device finds them in its bundle and its container, and the test app of Android finds them in its files.
class GpuTestFiles final {
  public:
    GpuTestFiles() = delete;

    [[nodiscard]] static std::filesystem::path getReferencesDirectory();
    [[nodiscard]] static std::filesystem::path getOutputDirectory();
};

} // namespace haylen::test
