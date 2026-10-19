#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "haylen/graphics/ImagePlaceholder.hpp"

// The entry point libFuzzer calls with each input it generates, which must either decode into a placeholder image or fail with the error of a malformed text.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        (void)haylen::graphics::ImagePlaceholder::decode(std::string_view(reinterpret_cast<const char*>(data), size), 8, 8);
    } catch (const std::invalid_argument&) {}
    return 0;
}
