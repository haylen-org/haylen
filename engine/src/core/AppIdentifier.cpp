#include "core/AppIdentifier.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <span>

#include "content/crypto/Hasher.hpp"

namespace haylen::core {

std::string AppIdentifier::fromLocation(std::string_view location) {
    // The name keeps its ASCII letters in lowercase and its digits, without the digits it starts with, since every part of an identifier starts with a letter.
    std::string letters;
    for (const char character : nameOf(location)) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'z';
        const bool upper = character >= 'A' && character <= 'Z';
        if (digit && letters.empty()) {
            continue;
        }

        if (digit || lower) {
            letters.push_back(character);
        } else if (upper) {
            letters.push_back(static_cast<char>(character - 'A' + 'a'));
        }

        if (letters.size() == kMaxName) {
            break;
        }
    }

    if (letters.empty()) {
        letters = kFallback;
    }

    const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(location.data()), location.size());
    std::array<std::uint8_t, kDigestSize> digest{};
    content::Hasher(kDigestSize).update(bytes).finish(digest);

    std::string identifier{kPrefix};
    identifier += letters;
    for (const std::uint8_t byte : digest) {
        identifier += std::format("{:02x}", byte);
    }

    return identifier;
}

std::string_view AppIdentifier::nameOf(std::string_view location) {
    std::string_view rest = location;
    if (rest.find("://") != std::string_view::npos) {
        rest = rest.substr(0, rest.find_first_of("?#"));
    }

    while (!rest.empty() && (rest.back() == '/' || rest.back() == '\\')) {
        rest.remove_suffix(1);
    }

    const std::size_t separator = rest.find_last_of("/\\");
    return separator == std::string_view::npos ? rest : rest.substr(separator + 1);
}

} // namespace haylen::core
