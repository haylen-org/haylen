#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "content/Error.hpp"
#include "content/crypto/Digest.hpp"

namespace haylen::content {

// Reads the fields that `BinaryWriter` writes from bytes that may be hostile. Every read is bounds-checked, and a read past the end or a broken rule throws an `Error` with the code and the name of the structure being read.
class BinaryReader final {
  public:
    BinaryReader(std::span<const std::uint8_t> source, Error::Code errorCode, std::string_view structure) noexcept;

    template <std::unsigned_integral T> [[nodiscard]] T read() {
        const std::span<const std::uint8_t> field = readBytes(sizeof(T));
        T value = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            value |= static_cast<T>(static_cast<T>(field[index]) << (8 * index));
        }
        return value;
    }

    [[nodiscard]] std::span<const std::uint8_t> readBytes(std::size_t count);
    [[nodiscard]] Digest readDigest();

    // Reads a string of at most `maximum` bytes.
    [[nodiscard]] std::string_view readString(std::size_t maximum);

    // Skips fields that were read already.
    void skip(std::size_t count);

    // Skips reserved bytes, which must be zero.
    void skipZeros(std::size_t count);
    void requireEnd() const;

    [[nodiscard]] std::size_t getPosition() const noexcept {
        return position;
    }
    [[nodiscard]] std::size_t getRemaining() const noexcept {
        return bytes.size() - position;
    }

    // Throws the error of the structure with a sentence that says what is wrong with it.
    [[noreturn]] void fail(std::string_view problem) const;

  private:
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
    Error::Code code;
    std::string name;
};

} // namespace haylen::content
