#include "haylen/core/JsonBytes.hpp"

#include <cstdint>
#include <format>
#include <stdexcept>
#include <vector>

namespace haylen::core {

Json JsonBytes::makeReference(std::size_t index) {
    return Json{{kKey, index}};
}

std::optional<std::size_t> JsonBytes::findReference(const Json& value) {
    if (!value.is_object() || value.size() != 1) {
        return std::nullopt;
    }
    const auto index = value.find(kKey);
    if (index == value.end()) {
        return std::nullopt;
    }
    if (index->is_number_unsigned()) {
        return index->get<std::size_t>();
    }
    if (index->is_number_integer() && index->get<std::int64_t>() >= 0) {
        return static_cast<std::size_t>(index->get<std::int64_t>());
    }
    return std::nullopt;
}

// The walks keep their own stack, so JSON nested deeper than the native stack allows never overflows it.
void JsonBytes::validate(const Json& value, std::size_t count) {
    std::vector<const Json*> pending{&value};
    while (!pending.empty()) {
        const Json& current = *pending.back();
        pending.pop_back();
        if (const std::optional<std::size_t> index = findReference(current)) {
            if (*index >= count) {
                throw std::invalid_argument(std::format("The JSON refers to byte buffer {}, but only {} came with it.", *index, count));
            }
            continue;
        }
        if (current.is_structured()) {
            for (const Json& element : current) {
                pending.push_back(&element);
            }
        }
    }
}

void JsonBytes::shift(Json& value, std::size_t offset) {
    std::vector<Json*> pending{&value};
    while (!pending.empty()) {
        Json& current = *pending.back();
        pending.pop_back();
        if (const std::optional<std::size_t> index = findReference(current)) {
            current = makeReference(*index + offset);
            continue;
        }
        if (current.is_structured()) {
            for (Json& element : current) {
                pending.push_back(&element);
            }
        }
    }
}

} // namespace haylen::core
