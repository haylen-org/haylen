#include "haylen/ui/PropertyReader.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <vector>

namespace haylen::ui {

PropertyReader::PropertyReader(const core::Json& values, std::string_view componentKind) : properties(values), kind(componentKind) {
    if (!values.is_object()) {
        throw std::invalid_argument("The properties of " + describeKind(kind) + " must be an object.");
    }
    used.insert(kStructuralKeys.begin(), kStructuralKeys.end());
}

bool PropertyReader::isList(const core::Json& value) noexcept {
    return value.is_array() || (value.is_object() && value.empty());
}

bool PropertyReader::has(std::string_view key) const {
    return properties.contains(key);
}

const core::Json* PropertyReader::take(std::string_view key) {
    const auto found = properties.find(key);
    if (found == properties.end()) {
        return nullptr;
    }
    used.emplace(key);
    return &*found;
}

void PropertyReader::fail(std::string_view key, std::string_view problem) const {
    throw std::invalid_argument(describeProperty(getQualifiedName(key)) + " " + std::string(problem) + ".");
}

std::string PropertyReader::getQualifiedName(std::string_view key) const {
    return kind + "." + std::string(key);
}

std::string PropertyReader::describeProperty(std::string_view path) {
    const std::size_t dot = path.find('.');
    return "The property \"" + std::string(path.substr(dot + 1)) + "\" of " + describeKind(path.substr(0, dot));
}

std::string PropertyReader::describeKind(std::string_view componentKind) {
    const bool vowel = !componentKind.empty() && std::string_view("aeiou").find(componentKind.front()) != std::string_view::npos;
    return (vowel ? "an \"" : "a \"") + std::string(componentKind) + "\"";
}

// Names the values a number may take, leaving out a maximum that only marks the limit of its type.
template <typename Number> std::string PropertyReader::describeRange(Number minimum, Number maximum) {
    if (maximum == std::numeric_limits<Number>::max()) {
        return std::format("must be at least {}", minimum);
    }
    return std::format("must be from {} to {}", minimum, maximum);
}

void PropertyReader::read(std::string_view key, bool& out) {
    if (const core::Json* value = take(key)) {
        if (!value->is_boolean()) {
            fail(key, "must be \"true\" or \"false\"");
        }
        out = value->get<bool>();
    }
}

void PropertyReader::read(std::string_view key, std::string& out) {
    if (const core::Json* value = take(key)) {
        if (!value->is_string()) {
            fail(key, "must be a string");
        }
        out = value->get<std::string>();
    }
}

void PropertyReader::read(std::string_view key, float& out, float minimum, float maximum) {
    if (const core::Json* value = take(key)) {
        if (!value->is_number() || !std::isfinite(value->get<double>())) {
            fail(key, "must be a number");
        }
        const auto number = value->get<float>();
        if (number < minimum || number > maximum) {
            fail(key, describeRange(minimum, maximum));
        }
        out = number;
    }
}

void PropertyReader::read(std::string_view key, double& out, double minimum, double maximum) {
    if (const core::Json* value = take(key)) {
        if (!value->is_number() || !std::isfinite(value->get<double>())) {
            fail(key, "must be a number");
        }
        const auto number = value->get<double>();
        if (number < minimum || number > maximum) {
            fail(key, describeRange(minimum, maximum));
        }
        out = number;
    }
}

void PropertyReader::read(std::string_view key, int& out, int minimum, int maximum) {
    if (const core::Json* value = take(key)) {
        if (!value->is_number() || std::trunc(value->get<double>()) != value->get<double>()) {
            fail(key, "must be a whole number");
        }
        const auto number = value->get<double>();
        if (number < minimum || number > maximum) {
            fail(key, describeRange(minimum, maximum));
        }
        out = static_cast<int>(number);
    }
}

void PropertyReader::read(std::string_view key, math::Color& out) {
    if (const core::Json* value = take(key)) {
        const std::optional<math::Color> color = value->is_string() ? math::Color::parse(value->get<std::string>()) : std::nullopt;
        if (!color) {
            fail(key, "must be a color such as \"#FF2E7D32\"");
        }
        out = *color;
    }
}

void PropertyReader::read(std::string_view key, TextValue& out) {
    if (const core::Json* value = take(key)) {
        out = TextValue::fromJson(*value, getQualifiedName(key));
    }
}

void PropertyReader::read(std::string_view key, math::Insets& out) {
    const core::Json* value = take(key);
    if (value == nullptr) {
        return;
    }

    std::vector<float> sides;
    if (value->is_number()) {
        sides.assign(4, value->get<float>());
    } else if (value->is_array() && (value->size() == 2 || value->size() == 4)) {
        for (const core::Json& side : *value) {
            sides.push_back(side.is_number() ? side.get<float>() : -1.0F);
        }
    }
    if (sides.empty() || std::ranges::any_of(sides, [](float side) { return !(side >= 0.0F) || !std::isfinite(side); })) {
        fail(key, "must be one, two or four non-negative numbers");
    }
    out = sides.size() == 2 ? math::Insets{.left = sides[1], .top = sides[0], .right = sides[1], .bottom = sides[0]} : math::Insets{.left = sides[3], .top = sides[0], .right = sides[1], .bottom = sides[2]};
}

void PropertyReader::read(std::string_view key, std::optional<text::Gradient>& out) {
    const core::Json* value = take(key);
    if (value == nullptr) {
        return;
    }
    if (!value->is_object() || value->contains("colors") == value->contains("stops")) {
        fail(key, "must be a table with \"colors\" or \"stops\" and an optional \"angle\", such as {colors = {\"#FFFFE9A0\", \"#FFC8861E\"}}");
    }
    for (const auto& [name, item] : value->items()) {
        if (name != "colors" && name != "stops" && name != "angle") {
            fail(key, std::format("has the unknown key \"{}\"", name));
        }
    }

    const bool paired = value->contains("stops");
    const core::Json& list = value->at(paired ? "stops" : "colors");
    if (!list.is_array() || list.empty() || list.size() > text::Gradient::kMaxStops) {
        fail(key, "holds from 1 to 4 colors");
    }
    std::vector<text::Gradient::Stop> stops;
    for (std::size_t index = 0; index < list.size(); ++index) {
        const core::Json& entry = paired && list[index].is_array() && list[index].size() == 2 ? list[index][1] : list[index];
        const std::optional<math::Color> color = entry.is_string() ? math::Color::parse(entry.get<std::string>()) : std::nullopt;
        if (!color || (paired && !list[index][0].is_number())) {
            fail(key, paired ? "lists its stops as pairs of an offset and a color, such as {0, \"#FFFFE9A0\"}" : "lists its colors as strings such as \"#FFFFE9A0\"");
        }
        const float spread = list.size() > 1 ? static_cast<float>(index) / static_cast<float>(list.size() - 1) : 0.0F;
        stops.push_back({.offset = paired ? list[index][0].get<float>() : spread, .color = *color});
    }

    const bool inRange = std::ranges::all_of(stops, [](const text::Gradient::Stop& stop) { return stop.offset >= 0.0F && stop.offset <= 1.0F; });
    if (!inRange || !std::ranges::is_sorted(stops, {}, &text::Gradient::Stop::offset)) {
        fail(key, "needs offsets from 0 to 1 in increasing order");
    }
    const core::Json* angle = value->contains("angle") ? &value->at("angle") : nullptr;
    if (angle != nullptr && !(angle->is_number() && std::isfinite(angle->get<float>()))) {
        fail(key, "needs an angle in radians");
    }
    out = text::Gradient(stops, angle != nullptr ? angle->get<float>() : text::Gradient::kDown);
}

void PropertyReader::read(std::string_view key, std::optional<Theme::Color>& out) {
    if (const core::Json* value = take(key)) {
        const std::optional<Theme::Color> role = value->is_string() ? Theme::colorFromName(value->get<std::string>()) : std::nullopt;
        if (!role) {
            fail(key, "must name a theme color such as \"accent\" or \"textMuted\"");
        }
        out = role;
    }
}

void PropertyReader::readLength(std::string_view key, std::optional<float>& out) {
    const core::Json* value = take(key);
    if (value == nullptr) {
        return;
    }
    if (value->is_string() && *value == "auto") {
        out.reset();
        return;
    }
    if (!value->is_number() || !(value->get<double>() >= 0.0) || !std::isfinite(value->get<double>())) {
        fail(key, "must be a non-negative number or \"auto\"");
    }
    out = value->get<float>();
}

void PropertyReader::finish() const {
    for (const auto& [key, value] : properties.items()) {
        if (!used.contains(key)) {
            fail(key, "does not exist");
        }
    }
}

} // namespace haylen::ui
