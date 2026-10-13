#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::core {

// Encodes and decodes MessagePack, the binary form of JSON that servers of online games often frame their messages in. Decoding checks the whole message before it builds any value, so a message that ends early, holds bytes after its value, nests deeper than `kMaxDepth`, uses extension types or claims more items than its bytes hold fails at once. Binary values of MessagePack become binary JSON values. Both methods run on any thread, which lets apps decode large messages off the frame thread.
class MessagePack final {
  public:
    static constexpr std::size_t kMaxDepth = 128;

    // Throws `std::invalid_argument` with `The bytes are not valid MessagePack: <reason>.`.
    [[nodiscard]] static Json decode(std::span<const std::byte> bytes);

    // Throws `std::invalid_argument` with `The value cannot be encoded as MessagePack: <reason>.` for a value nested deeper than `kMaxDepth`.
    [[nodiscard]] static std::vector<std::byte> encode(const Json& value);

  private:
    // Walks the structure of a message without recursion and returns the reason it is not valid, or null.
    [[nodiscard]] static const char* check(std::span<const std::byte> bytes) noexcept;
    [[nodiscard]] static bool isTooDeep(const Json& value, std::size_t depth) noexcept;
};

} // namespace haylen::core
