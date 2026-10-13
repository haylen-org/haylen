#include "haylen/core/MessagePack.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace haylen::core {

// Every value is one byte of type and the bytes that type announces. Containers push the count of their items, which the walk takes one by one, and an item count beyond the bytes that are left cannot be honest, since every item takes at least one byte.
const char* MessagePack::check(std::span<const std::byte> bytes) noexcept {
    std::vector<std::uint64_t> pending;
    std::size_t offset = 0;
    std::uint64_t expected = 1;
    // clang-format off
    const auto read = [&bytes, &offset](std::size_t size, std::uint64_t& value) {
        if (bytes.size() - offset < size) {
            return false;
        }
        value = 0;
        for (std::size_t index = 0; index < size; ++index) {
            value = (value << 8U) | static_cast<std::uint8_t>(bytes[offset++]);
        }
        return true;
    };
    const auto skip = [&bytes, &offset](std::uint64_t size) {
        if (bytes.size() - offset < size) {
            return false;
        }
        offset += static_cast<std::size_t>(size);
        return true;
    };
    // clang-format on

    while (expected > 0 || !pending.empty()) {
        if (expected == 0) {
            expected = pending.back();
            pending.pop_back();
            continue;
        }
        --expected;
        if (offset >= bytes.size()) {
            return "it ends before its last value";
        }
        const auto type = static_cast<std::uint8_t>(bytes[offset++]);
        std::uint64_t length = 0;
        std::uint64_t items = 0;
        bool container = false;
        if (type <= 0x7F || type >= 0xE0 || type == 0xC0 || type == 0xC2 || type == 0xC3) {
            continue;
        }
        if (type >= 0x80 && type <= 0x8F) {
            items = static_cast<std::uint64_t>(type & 0x0FU) * 2;
            container = true;
        } else if (type >= 0x90 && type <= 0x9F) {
            items = type & 0x0FU;
            container = true;
        } else if (type >= 0xA0 && type <= 0xBF) {
            length = type & 0x1FU;
        } else {
            switch (type) {
            case 0xC4:
            case 0xD9:
                if (!read(1, length)) {
                    return "it ends inside a length";
                }
                break;
            case 0xC5:
            case 0xDA:
                if (!read(2, length)) {
                    return "it ends inside a length";
                }
                break;
            case 0xC6:
            case 0xDB:
                if (!read(4, length)) {
                    return "it ends inside a length";
                }
                break;
            case 0xCA:
                length = 4;
                break;
            case 0xCB:
                length = 8;
                break;
            case 0xCC:
            case 0xD0:
                length = 1;
                break;
            case 0xCD:
            case 0xD1:
                length = 2;
                break;
            case 0xCE:
            case 0xD2:
                length = 4;
                break;
            case 0xCF:
            case 0xD3:
                length = 8;
                break;
            case 0xDC:
            case 0xDE:
                if (!read(2, items)) {
                    return "it ends inside a count";
                }
                items *= type == 0xDE ? 2 : 1;
                container = true;
                break;
            case 0xDD:
            case 0xDF:
                if (!read(4, items)) {
                    return "it ends inside a count";
                }
                items *= type == 0xDF ? 2 : 1;
                container = true;
                break;
            default:
                return type == 0xC1 ? "it holds the byte 0xc1, which no value uses" : "it uses extension types, which the engine does not read";
            }
        }
        if (!container) {
            if (!skip(length)) {
                return "a value runs past its end";
            }
            continue;
        }
        if (items > bytes.size() - offset) {
            return "a container claims more items than its bytes hold";
        }
        if (items == 0) {
            continue;
        }
        if (pending.size() >= kMaxDepth) {
            return "it nests deeper than 128 levels";
        }
        pending.push_back(expected);
        expected = items;
    }
    return offset == bytes.size() ? nullptr : "bytes follow its value";
}

Json MessagePack::decode(std::span<const std::byte> bytes) {
    if (const char* reason = check(bytes)) {
        throw std::invalid_argument(std::string("The bytes are not valid MessagePack: ") + reason + ".");
    }
    const auto* first = reinterpret_cast<const std::uint8_t*>(bytes.data());
    try {
        return Json::from_msgpack(first, first + bytes.size(), true, true);
    } catch (const Json::exception& error) {
        throw std::invalid_argument(std::string("The bytes are not valid MessagePack: ") + error.what() + ".");
    }
}

bool MessagePack::isTooDeep(const Json& value, std::size_t depth) noexcept {
    if (depth > kMaxDepth) {
        return true;
    }
    if (!value.is_structured()) {
        return false;
    }
    for (const Json& element : value) {
        if (isTooDeep(element, depth + 1)) {
            return true;
        }
    }
    return false;
}

std::vector<std::byte> MessagePack::encode(const Json& value) {
    if (isTooDeep(value, 1)) {
        throw std::invalid_argument("The value cannot be encoded as MessagePack: it nests deeper than 128 levels.");
    }
    const std::vector<std::uint8_t> encoded = Json::to_msgpack(value);
    std::vector<std::byte> bytes(encoded.size());
    std::transform(encoded.begin(), encoded.end(), bytes.begin(), [](std::uint8_t byte) { return static_cast<std::byte>(byte); });
    return bytes;
}

} // namespace haylen::core
