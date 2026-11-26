#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "support/DamagedNavMeshes.hpp"

// The entry point libFuzzer calls with each input it generates, which damages the room of `test::DamagedNavMeshes` or stands for a file of its own, so the inputs reach every part of the reader of navigation meshes and the queries on what it reads.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::vector<std::uint8_t> bytes = haylen::test::DamagedNavMeshes::damage(std::span(data, size));
    haylen::test::DamagedNavMeshes::read(bytes);
    return 0;
}
