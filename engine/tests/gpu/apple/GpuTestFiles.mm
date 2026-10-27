#include "gpu/support/GpuTestFiles.hpp"

#import <Foundation/Foundation.h>

namespace haylen::test {

std::filesystem::path GpuTestFiles::getReferencesDirectory() {
    return std::filesystem::path(NSBundle.mainBundle.resourcePath.fileSystemRepresentation) / "gpu";
}

// The caches are the one folder of the container that tvOS lets an app write, and `haylen.py` copies the folder back from there.
std::filesystem::path GpuTestFiles::getOutputDirectory() {
    return std::filesystem::path(NSHomeDirectory().fileSystemRepresentation) / "Library" / "Caches" / "gpu-test-output";
}

} // namespace haylen::test
