#include "haylen/core/Utf8.hpp"

#include <cstdint>

namespace haylen::core {

bool Utf8::isContinuation(unsigned char byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

char32_t Utf8::decode(std::string_view text, std::size_t& offset) noexcept {
    const auto lead = static_cast<unsigned char>(text[offset++]);
    if (lead < 0x80U) {
        return lead;
    }

    std::size_t extra = 0;
    std::uint32_t codePoint = 0;
    std::uint32_t minimum = 0;
    if ((lead & 0xE0U) == 0xC0U) {
        extra = 1;
        codePoint = lead & 0x1FU;
        minimum = 0x80;
    } else if ((lead & 0xF0U) == 0xE0U) {
        extra = 2;
        codePoint = lead & 0x0FU;
        minimum = 0x800;
    } else if ((lead & 0xF8U) == 0xF0U) {
        extra = 3;
        codePoint = lead & 0x07U;
        minimum = 0x10000;
    } else {
        return kReplacementCharacter;
    }

    for (std::size_t index = 0; index < extra; ++index) {
        if (offset >= text.size() || !isContinuation(static_cast<unsigned char>(text[offset]))) {
            return kReplacementCharacter;
        }
        codePoint = (codePoint << 6U) | (static_cast<unsigned char>(text[offset++]) & 0x3FU);
    }

    const bool surrogate = codePoint >= 0xD800U && codePoint <= 0xDFFFU;
    if (codePoint < minimum || codePoint > 0x10FFFFU || surrogate) {
        return kReplacementCharacter;
    }
    return static_cast<char32_t>(codePoint);
}

std::u32string Utf8::decode(std::string_view text) {
    std::u32string result;
    result.reserve(text.size());
    for (std::size_t offset = 0; offset < text.size();) {
        result.push_back(decode(text, offset));
    }
    return result;
}

bool Utf8::isValid(std::string_view text) noexcept {
    // Only the three bytes that encode U+FFFD decode to it without an error.
    static constexpr std::string_view kEncodedReplacement = "\xEF\xBF\xBD";
    for (std::size_t offset = 0; offset < text.size();) {
        const std::size_t start = offset;
        if (decode(text, offset) == kReplacementCharacter && text.substr(start, offset - start) != kEncodedReplacement) {
            return false;
        }
    }
    return true;
}

void Utf8::append(std::string& output, char32_t codePoint) {
    auto value = static_cast<std::uint32_t>(codePoint);
    if (value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
        value = static_cast<std::uint32_t>(kReplacementCharacter);
    }

    if (value < 0x80U) {
        output.push_back(static_cast<char>(value));
    } else if (value < 0x800U) {
        output.push_back(static_cast<char>(0xC0U | (value >> 6U)));
        output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else if (value < 0x10000U) {
        output.push_back(static_cast<char>(0xE0U | (value >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (value >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((value >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    }
}

std::size_t Utf8::countCodePoints(std::string_view text) noexcept {
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        (void)decode(text, offset);
        ++count;
    }
    return count;
}

std::size_t Utf8::getOffset(std::string_view text, std::size_t index) noexcept {
    std::size_t offset = 0;
    for (std::size_t count = 0; count < index && offset < text.size(); ++count) {
        (void)decode(text, offset);
    }
    return offset;
}

} // namespace haylen::core
