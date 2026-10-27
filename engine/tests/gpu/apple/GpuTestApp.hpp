#pragma once

namespace haylen::test {

// Runs the GPU tests as an app of an iOS or tvOS device, since a device starts no plain executable. The tests run once the scene of the app is active, because Metal takes no work from an app in the background. They write their results as `results.xml` next to the images of the scenes that differ, and the app quits with their status.
class GpuTestApp final {
  public:
    GpuTestApp() = delete;

    static int run(int argc, char* argv[]);
    [[noreturn]] static void runTests();

  private:
    static inline int argumentCount = 0;
    static inline char** arguments = nullptr;
};

} // namespace haylen::test
