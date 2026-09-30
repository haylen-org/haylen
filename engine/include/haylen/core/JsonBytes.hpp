#pragma once

#include <cstddef>
#include <optional>

#include "haylen/core/Json.hpp"

namespace haylen::core {

// JSON that travels with byte buffers refers to buffer `N` as the object `{"$bytes": N}`, so binary data never turns into text.
class JsonBytes final {
  public:
    static constexpr const char* kKey = "$bytes";

    [[nodiscard]] static Json makeReference(std::size_t index);

    // Returns the buffer that the value refers to, or nothing when the value is no reference: an object whose only key is `$bytes` with an integer of 0 or more.
    [[nodiscard]] static std::optional<std::size_t> findReference(const Json& value);

    // Throws `std::invalid_argument` when the value refers to a buffer at or past `count`.
    static void validate(const Json& value, std::size_t count);

    // Adds `offset` to every reference, for JSON whose buffers move behind the buffers of other JSON in one list.
    static void shift(Json& value, std::size_t offset);
};

} // namespace haylen::core
