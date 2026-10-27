#include "gpu/apple/GpuTestApp.hpp"

#include <gtest/gtest.h>

#import <UIKit/UIKit.h>

#include <cstdlib>
#include <filesystem>

#include "gpu/support/GpuTestFiles.hpp"

namespace haylen::test {

int GpuTestApp::run(int argc, char* argv[]) {
    argumentCount = argc;
    arguments = argv;
    return UIApplicationMain(argc, argv, nil, nil);
}

void GpuTestApp::runTests() {
    // The container outlives the installs of the app, so the results of an earlier run go first.
    const std::filesystem::path output = GpuTestFiles::getOutputDirectory();
    std::filesystem::remove_all(output);

    GTEST_FLAG_SET(output, "xml:" + (output / "results.xml").string());
    testing::InitGoogleTest(&argumentCount, arguments);
    std::exit(RUN_ALL_TESTS());
}

} // namespace haylen::test
