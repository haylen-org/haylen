#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "haylen/graphics/Image.hpp"

// The entry point libFuzzer calls with each input it generates. The first byte picks a format, PNG, JPEG, GIF, BMP or TGA, and the bytes after it follow the signature of that format, which TGA lacks, so every decoder reads past its signature. The decoder must either make an image of at most 1024 pixels on a side, which bleeds and halves into mipmaps, or fail with the error of data that is not such an image.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static constexpr std::array<std::string_view, 5> kSignatures = {std::string_view("\x89PNG\r\n\x1A\n", 8), "\xFF\xD8\xFF", "GIF89a", "BM", ""};
    if (size == 0) {
        return 0;
    }

    const std::string_view signature = kSignatures[data[0] % kSignatures.size()];
    std::vector<std::uint8_t> encoded(signature.begin(), signature.end());
    encoded.insert(encoded.end(), data + 1, data + size);
    try {
        haylen::graphics::Image image = haylen::graphics::Image::decode(encoded, 1024);
        image.bleed();
        image.buildMipmaps();
    } catch (const std::runtime_error&) {}
    return 0;
}
