#include "core/AppIdentifier.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <span>

#include "content/crypto/Hasher.hpp"

namespace haylen::core {

std::string AppIdentifier::generate(const std::filesystem::path& location) {
    const std::filesystem::path resolved = std::filesystem::canonical(location);
    const std::u8string name = resolved.filename().u8string();
    const std::u8string path = resolved.u8string();
    return compose({reinterpret_cast<const char*>(name.data()), name.size()}, {reinterpret_cast<const char*>(path.data()), path.size()});
}

std::string AppIdentifier::compose(std::string_view name, std::string_view resolvedPath) {
    // The name keeps its ASCII letters in lowercase and its digits, without the digits it starts with, since every part of an identifier starts with a letter.
    std::string letters;
    for (const char character : name) {
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

    const std::span<const std::uint8_t> path(reinterpret_cast<const std::uint8_t*>(resolvedPath.data()), resolvedPath.size());
    std::array<std::uint8_t, kDigestSize> digest{};
    content::Hasher(kDigestSize).update(path).finish(digest);

    std::string identifier{kPrefix};
    identifier += letters;
    for (const std::uint8_t byte : digest) {
        identifier += std::format("{:02x}", byte);
    }

    return identifier;
}

} // namespace haylen::core
