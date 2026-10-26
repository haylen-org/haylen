#pragma once

#include <filesystem>
#include <string_view>

#include "haylen/graphics/Image.hpp"

namespace haylen::test {

// Compares the images that GPU devices draw with the pixels a test expects and with the reference images in `engine/tests/data/gpu`, which hold for every graphics API. A run with the environment variable `HAYLEN_GPU_REFERENCES` set to `write` writes the references instead of comparing with them, and a run that fails keeps the image it drew and the difference in `gpu-test-output` of the build tree.
class ImageComparison final {
  public:
    // How many pixels differ in a channel by more than the tolerance, and the largest difference of any channel.
    struct Result {
        int differing = 0;
        int largest = 0;
    };

    // Two images of the same size, compared channel by channel.
    [[nodiscard]] static Result compare(const graphics::Image& actual, const graphics::Image& expected, int tolerance);

    // Fails the running test when the image differs from the reference of the name by more than the tolerance in more than the allowed pixels, or when no reference exists.
    static void expectReference(const graphics::Image& actual, std::string_view name, int tolerance, int allowedPixels);

  private:
    [[nodiscard]] static bool isWritingReferences();
    [[nodiscard]] static graphics::Image makeDifference(const graphics::Image& actual, const graphics::Image& expected);
    static void write(const graphics::Image& image, const std::filesystem::path& path);
};

} // namespace haylen::test
