#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

#include "haylen/core/MessagePack.hpp"

// The entry point libFuzzer calls with each input it generates, which must either decode into a value that encodes again or fail with the error of bytes that are not valid MessagePack.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        (void)haylen::core::MessagePack::encode(haylen::core::MessagePack::decode(std::as_bytes(std::span(data, size))));
    } catch (const std::invalid_argument&) {}
    return 0;
}
