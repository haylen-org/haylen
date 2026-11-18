#include "core/Base64.hpp"

#include <cctype>
#include <cstddef>

namespace haylen::core {

std::array<int, 256> Base64::makeTable() noexcept {
    std::array<int, 256> table{};
    table.fill(-1);
    constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t index = 0; index < kAlphabet.size(); ++index) {
        table[static_cast<unsigned char>(kAlphabet[index])] = static_cast<int>(index);
    }

    return table;
}

std::optional<std::vector<std::uint8_t>> Base64::decode(std::string_view text) {
    static const std::array<int, 256> table = makeTable();
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 4 * 3);
    std::uint32_t buffer = 0;
    int bits = 0;
    std::size_t symbols = 0;
    std::size_t padding = 0;
    for (const char character : text) {
        if (std::isspace(static_cast<unsigned char>(character)) != 0) {
            continue;
        }

        if (character == '=') {
            ++padding;
            continue;
        }

        const int value = table[static_cast<unsigned char>(character)];
        if (value < 0 || padding > 0) {
            return std::nullopt;
        }

        ++symbols;
        buffer = (buffer << 6U) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((buffer >> static_cast<unsigned>(bits)) & 0xFFU));
        }
    }

    // Every 4 symbols hold 3 bytes, a last group of 1 symbol holds less than a byte, and padding only fills the last group to 4 symbols.
    const std::size_t rest = symbols % 4;
    const bool paddingFits = padding == 0 || (rest != 0 && rest + padding == 4);
    if (rest == 1 || !paddingFits) {
        return std::nullopt;
    }

    return bytes;
}

} // namespace haylen::core
