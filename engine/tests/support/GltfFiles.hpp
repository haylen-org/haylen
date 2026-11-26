#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::test {

// Builds the glTF files that tests change one rule at a time: a triangle with positions, normals and indices in the binary chunk of a GLB file, and the GLB file of any JSON and binary chunk, so a test of a rule names only what it breaks.
class GltfFiles final {
  public:
    // A document and the bytes of its binary chunk.
    struct File {
        core::Json document;
        std::vector<std::uint8_t> binary;
    };

    // The folder of the package of the test files of `engine/tests/data/gltf`, whose content folder holds them.
    [[nodiscard]] static std::filesystem::path getFolder();

    // A triangle of three vertices at (0, 0, 0), (1, 0, 0) and (0, 1, 0) facing positive `z`, in buffer views 0 for positions, 1 for normals and 2 for unsigned short indices, read by accessors 0, 1 and 2 of the one primitive of mesh 0, which node 0 of scene 0 places.
    [[nodiscard]] static File triangle();

    // Packs a document and its binary chunk as a GLB file, with the JSON padded by spaces and the binary chunk by zeros, and without a binary chunk when the bytes are empty.
    [[nodiscard]] static std::vector<std::uint8_t> glb(const core::Json& document, std::span<const std::uint8_t> binary);
    [[nodiscard]] static std::vector<std::uint8_t> glb(const File& file);

    // Appends floats or unsigned shorts to the binary chunk, padded to four bytes, and returns the offset they start at.
    static std::uint32_t append(std::vector<std::uint8_t>& binary, std::span<const float> values);
    static std::uint32_t append(std::vector<std::uint8_t>& binary, std::span<const std::uint16_t> values);

  private:
    static void appendWord(std::vector<std::uint8_t>& bytes, std::uint32_t value);
    static void pad(std::vector<std::uint8_t>& bytes, std::uint8_t fill);
};

} // namespace haylen::test
