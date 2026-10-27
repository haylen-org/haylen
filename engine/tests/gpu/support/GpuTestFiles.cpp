#include "gpu/support/GpuTestFiles.hpp"

namespace haylen::test {

std::filesystem::path GpuTestFiles::getReferencesDirectory() {
    return HAYLEN_GPU_REFERENCES_DIR;
}

std::filesystem::path GpuTestFiles::getOutputDirectory() {
    return HAYLEN_GPU_TEST_OUTPUT;
}

} // namespace haylen::test
