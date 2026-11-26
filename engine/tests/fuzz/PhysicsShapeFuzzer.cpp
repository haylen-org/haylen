#include <cstddef>
#include <cstdint>
#include <span>

#include "support/PhysicsShapeInputs.hpp"

// The entry point libFuzzer calls with each input it generates, which must either make a shape or fail with the sentence of data that makes none.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static_cast<void>(haylen::test::PhysicsShapeInputs::build(std::span(data, size)));
    return 0;
}
