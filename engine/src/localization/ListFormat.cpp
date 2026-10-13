#include "haylen/localization/ListFormat.hpp"

#include <algorithm>

#include "localization/LocaleData.hpp"

namespace haylen::localization {

const std::array<std::pair<std::string_view, ListFormat::Type>, 2> ListFormat::kTypeNames{{{"and", Type::And}, {"or", Type::Or}}};

std::optional<ListFormat::Type> ListFormat::typeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kTypeNames, name, &std::pair<std::string_view, Type>::first);
    return found != kTypeNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view ListFormat::typeName(Type value) noexcept {
    return std::ranges::find(kTypeNames, value, &std::pair<std::string_view, Type>::second)->first;
}

// Two items join with the word of the type, and longer lists separate the items and put the word before the last one, with the serial comma where the language writes it.
std::string ListFormat::format(std::string_view language, std::span<const std::string> items, Type type) {
    const LocaleData::Locale& locale = LocaleData::find(language);
    std::string text;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            const bool last = index + 1 == items.size();
            if (!last) {
                text.append(locale.listSeparator);
            } else if (items.size() == 2) {
                text.append(type == Type::And ? locale.andTwo : locale.orTwo);
            } else {
                text.append(type == Type::And ? locale.andLast : locale.orLast);
            }
        }
        text += items[index];
    }
    return text;
}

} // namespace haylen::localization
