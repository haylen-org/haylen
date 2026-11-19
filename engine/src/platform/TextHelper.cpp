#include "platform/TextHelper.hpp"

#include "haylen/core/Utf8.hpp"

namespace haylen::platform {

std::string TextHelper::toValidUtf8(std::string_view text) {
    if (core::Utf8::isValid(text)) {
        return std::string(text);
    }

    std::string valid;
    valid.reserve(text.size());
    for (std::size_t offset = 0; offset < text.size();) {
        core::Utf8::append(valid, core::Utf8::decode(text, offset));
    }

    return valid;
}

} // namespace haylen::platform
