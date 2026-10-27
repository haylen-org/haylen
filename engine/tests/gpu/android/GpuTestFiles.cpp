#include "gpu/support/GpuTestFiles.hpp"

#include "gpu/android/GpuTestActivity.hpp"

namespace haylen::test {

// The app copies the reference images from its assets into its files, which also keep the images of the scenes that differ for `run-as` to copy back.
std::filesystem::path GpuTestFiles::getReferencesDirectory() {
    return GpuTestActivity::getFilesDirectory() / "gpu";
}

std::filesystem::path GpuTestFiles::getOutputDirectory() {
    return GpuTestActivity::getFilesDirectory() / "gpu-test-output";
}

} // namespace haylen::test
