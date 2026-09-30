#include "haylen/ui/TextValue.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/ui/PropertyReader.hpp"

namespace haylen::ui {

std::string TextValue::formatNumber(const core::Json& value) {
    const double number = value.get<double>();
    if (std::trunc(number) == number && std::abs(number) < 1e15) {
        return std::to_string(static_cast<long long>(number));
    }
    return value.dump();
}

TextValue TextValue::fromJson(const core::Json& value, std::string_view context) {
    if (value.is_string()) {
        return {.literal = value.get<std::string>()};
    }
    if (value.is_number()) {
        return {.literal = formatNumber(value)};
    }
    if (value.is_object()) {
        core::JsonValidator::requireKnownKeys(value, {"key", "args"}, "\"" + std::string(context) + "\"");
        const auto found = value.find("key");
        const auto values = value.find("args");
        if (found == value.end() || !found->is_string() || found->get<std::string>().empty() || (values != value.end() && !values->is_object())) {
            throw std::invalid_argument(PropertyReader::describeProperty(context) + " needs a translation key and an optional \"args\" object.");
        }
        return {.literal = {}, .key = found->get<std::string>(), .arguments = values != value.end() ? *values : core::Json::object()};
    }
    throw std::invalid_argument(PropertyReader::describeProperty(context) + " must be text, a number or a translation such as \"{key = 'menu.play'}\".");
}

} // namespace haylen::ui
