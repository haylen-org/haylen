#include <cstddef>
#include <cstdint>

#include "support/ContentParsers.hpp"

// The entry point libFuzzer calls with each input it generates.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    haylen::test::ContentParsers::parse({data, size});
    return 0;
}
