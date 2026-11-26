#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace haylen::test {

// Damages the bytes of the `.navmesh` file of one small room, with obstacles or without, as the fuzzer of navigation meshes does, so a test replays an input that the fuzzer found.
class DamagedNavMeshes final {
  public:
    // The bytes of the room: a floor with a wall, a door, a road and a link, baked once for each kind.
    [[nodiscard]] static const std::vector<std::uint8_t>& getRoom(bool obstacles);

    // An odd first byte keeps the rest of the input as a file. An even one flips the bits of the rest of the input into the room with obstacles when its second byte is odd, from the offset its next two bytes name.
    [[nodiscard]] static std::vector<std::uint8_t> damage(std::span<const std::uint8_t> input);

    // Reads the bytes as a `.navmesh` file and asks the mesh that reads for a path, a ray, random points and its triangles. The reader may refuse a file only as one that is damaged.
    static void read(std::span<const std::uint8_t> bytes);

  private:
    [[nodiscard]] static std::vector<std::uint8_t> bake(bool obstacles);
};

} // namespace haylen::test
