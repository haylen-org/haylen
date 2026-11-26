#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "haylen/3d/gltf/Document.hpp"

namespace haylen::gltf {

// Reads the elements of an accessor of a document as the numbers they stand for: across the stride of its buffer view, with the padding of the columns of matrices left out, with the values of its sparse part in place and, for normalized integers, as the floats they encode.
class AccessorReader final {
  public:
    // Returns `count` times the components of the type, as floats, with each matrix by columns. Throws `std::out_of_range` for an accessor the document lacks and `std::invalid_argument` for an accessor whose data lies outside its buffers or would give more values than the limit of a buffer.
    [[nodiscard]] static std::vector<float> readFloats(const Document& document, std::size_t accessor);

    // Returns the values of an accessor of unsigned integers that are not normalized, such as indices and joints. Throws as `readFloats` does, and `std::invalid_argument` for an accessor of another kind.
    [[nodiscard]] static std::vector<std::uint32_t> readUnsigned(const Document& document, std::size_t accessor);
};

} // namespace haylen::gltf
