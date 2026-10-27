#include "gpu/support/ImageComparison.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "support/Environment.hpp"

namespace haylen::test {

ImageComparison::Result ImageComparison::compare(const graphics::Image& actual, const graphics::Image& expected, int tolerance) {
    if (actual.getWidth() != expected.getWidth() || actual.getHeight() != expected.getHeight()) {
        return {.differing = std::max(actual.getWidth() * actual.getHeight(), expected.getWidth() * expected.getHeight()), .largest = 255};
    }
    Result result;
    const std::span<const std::uint8_t> drawn = actual.getPixels();
    const std::span<const std::uint8_t> wanted = expected.getPixels();
    for (std::size_t pixel = 0; pixel < drawn.size(); pixel += 4) {
        int largest = 0;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            largest = std::max(largest, std::abs(static_cast<int>(drawn[pixel + channel]) - static_cast<int>(wanted[pixel + channel])));
        }
        result.largest = std::max(result.largest, largest);
        result.differing += largest > tolerance ? 1 : 0;
    }
    return result;
}

bool ImageComparison::isWritingReferences() {
    return Environment::get("HAYLEN_GPU_REFERENCES") == "write";
}

// The difference shows each pixel in the gray of its largest difference, opaque, so a pixel that differs stands out on black.
graphics::Image ImageComparison::makeDifference(const graphics::Image& actual, const graphics::Image& expected) {
    graphics::Image difference(actual.getWidth(), actual.getHeight(), math::Color::black());
    const std::span<const std::uint8_t> drawn = actual.getPixels();
    const std::span<const std::uint8_t> wanted = expected.getPixels();
    std::span<std::uint8_t> shown = difference.getWritablePixels();
    for (std::size_t pixel = 0; pixel < shown.size(); pixel += 4) {
        int largest = 0;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            largest = std::max(largest, std::abs(static_cast<int>(drawn[pixel + channel]) - static_cast<int>(wanted[pixel + channel])));
        }
        std::fill_n(shown.begin() + static_cast<std::ptrdiff_t>(pixel), 3, static_cast<std::uint8_t>(largest));
    }
    return difference;
}

void ImageComparison::write(const graphics::Image& image, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    const std::vector<std::uint8_t> encoded = image.encodePng();
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
}

void ImageComparison::expectReference(const graphics::Image& actual, std::string_view name, int tolerance, int allowedPixels) {
    const std::filesystem::path reference = std::filesystem::path(HAYLEN_GPU_REFERENCES_DIR) / (std::string(name) + ".png");
    if (isWritingReferences()) {
        write(actual, reference);
        std::cout << "Wrote the reference image \"" << reference.generic_string() << "\". Look at it before you commit it.\n";
        return;
    }
    if (!std::filesystem::exists(reference)) {
        ADD_FAILURE() << "The reference image \"" << reference.generic_string() << "\" does not exist. Run the GPU tests with \"HAYLEN_GPU_REFERENCES=write\" on a device that draws the scene right, look at the image and commit it.";
        return;
    }

    std::ifstream file(reference, std::ios::binary);
    const std::vector<std::uint8_t> encoded{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const graphics::Image expected = graphics::Image::decode(encoded);
    const Result result = compare(actual, expected, tolerance);
    if (result.differing <= allowedPixels) {
        return;
    }

    const std::filesystem::path output = std::filesystem::path(HAYLEN_GPU_TEST_OUTPUT) / std::string(name);
    write(actual, output / "drawn.png");
    if (actual.getWidth() == expected.getWidth() && actual.getHeight() == expected.getHeight()) {
        write(makeDifference(actual, expected), output / "difference.png");
    }
    ADD_FAILURE() << "The image of \"" << name << "\" differs from its reference in " << result.differing << " pixels, by up to " << result.largest << ", and allows " << allowedPixels << " pixels past " << tolerance << ". The image it drew and the difference are in \"" << output.generic_string() << "\".";
}

} // namespace haylen::test
