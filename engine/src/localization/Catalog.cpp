#include "haylen/localization/Catalog.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

#include "haylen/localization/PluralRules.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::localization {

bool Catalog::isPlural(const core::Json& value) {
    if (!value.is_object() || !value.contains("other")) {
        return false;
    }
    for (const auto& [form, text] : value.items()) {
        if (!PluralRules::categoryFromName(form) || !text.is_string()) {
            return false;
        }
    }
    return true;
}

void Catalog::flatten(const core::Json& table, const std::string& prefix, Table& entries) {
    for (const auto& [key, value] : table.items()) {
        const std::string path = prefix.empty() ? key : prefix + "." + key;
        if (value.is_string() || isPlural(value)) {
            entries[path] = value;
        } else if (value.is_object()) {
            flatten(value, path, entries);
        } else {
            throw std::invalid_argument("The localization entry \"" + path + "\" must be text, a plural form or a group of entries.");
        }
    }
}

std::string Catalog::normalizeTag(std::string_view tag) {
    std::string normalized(tag);
    for (char& character : normalized) {
        character = character == '_' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return normalized;
}

std::string_view Catalog::getBaseLanguage(std::string_view tag) {
    return tag.substr(0, tag.find('-'));
}

std::string Catalog::formatArgument(const core::Json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_number_float()) {
        const double number = value.get<double>();
        if (std::trunc(number) == number && std::abs(number) < 1e15) {
            return std::to_string(static_cast<long long>(number));
        }
    }
    return value.dump();
}

void Catalog::checkArguments(const core::Json& arguments) {
    if (!arguments.is_object()) {
        throw std::invalid_argument("Localization arguments must be a JSON object.");
    }
}

std::string Catalog::format(std::string_view pattern, const core::Json& arguments) {
    checkArguments(arguments);
    std::string result;
    result.reserve(pattern.size());
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        const char character = pattern[index];
        const bool doubled = index + 1 < pattern.size() && pattern[index + 1] == character;
        if ((character == '{' || character == '}') && doubled) {
            result += character;
            ++index;
            continue;
        }

        const std::size_t close = character == '{' ? pattern.find('}', index + 1) : std::string_view::npos;
        if (close == std::string_view::npos) {
            result += character;
            continue;
        }

        // A placeholder without a matching argument stays in the text, where it is easy to spot.
        const std::string name(pattern.substr(index + 1, close - index - 1));
        const auto argument = arguments.find(name);
        result += argument != arguments.end() ? formatArgument(*argument) : std::string(pattern.substr(index, close - index + 1));
        index = close;
    }
    return result;
}

// Letters take accents, placeholders and the tags of rich text stay as they are, and dots after the text make it a third longer.
std::string Catalog::translatePseudo(std::string_view pattern) {
    constexpr std::string_view kPlain = "aeiouyncAEIOUYNC";
    constexpr std::array<std::string_view, kPlain.size()> kAccented{"á", "é", "í", "ó", "ú", "ý", "ñ", "ç", "Á", "É", "Í", "Ó", "Ú", "Ý", "Ñ", "Ç"};
    std::string result = "« ";
    result.reserve(pattern.size() * 2 + 8);
    std::size_t letters = 0;
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        const char character = pattern[index];
        const std::size_t close = character == '{' || character == '[' ? pattern.find(character == '{' ? '}' : ']', index + 1) : std::string_view::npos;
        if (close != std::string_view::npos) {
            result.append(pattern.substr(index, close - index + 1));
            index = close;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(character)) != 0) {
            ++letters;
        }
        const std::size_t plain = kPlain.find(character);
        if (plain == std::string_view::npos) {
            result += character;
        } else {
            result.append(kAccented[plain]);
        }
    }
    result += ' ';
    result.append((letters + 2) / 3, '.');
    result += " »";
    return result;
}

void Catalog::add(const std::string& name, const core::Json& table) {
    if (name.empty()) {
        throw std::invalid_argument("A localization table needs a language.");
    }
    if (!table.is_object()) {
        throw std::invalid_argument("The localization table of \"" + name + "\" must be a JSON object.");
    }

    std::optional<text::Direction> direction;
    if (const auto declared = table.find(kDirectionKey); declared != table.end()) {
        direction = declared->is_string() ? text::Style::directionFromName(declared->get<std::string>()) : std::nullopt;
        if (!direction || *direction == text::Direction::Auto) {
            throw std::invalid_argument("The \"@direction\" of the localization table of \"" + name + "\" must be \"leftToRight\" or \"rightToLeft\".");
        }
    }
    core::Json texts = table;
    texts.erase(std::string(kDirectionKey));

    Table entries = tables.contains(name) ? tables.find(name)->second : Table{};
    flatten(texts, "", entries);
    tables[name] = std::move(entries);
    if (direction) {
        directions.insert_or_assign(name, *direction);
    }

    if (language.empty()) {
        language = name;
    }
    if (fallback.empty()) {
        fallback = name;
    }
}

void Catalog::requireLanguage(std::string_view name) const {
    if (!tables.contains(name)) {
        throw std::invalid_argument("No localization table was added for \"" + std::string(name) + "\".");
    }
}

void Catalog::setLanguage(std::string_view value) {
    requireLanguage(value);
    language = value;
}

void Catalog::setFallback(std::string_view value) {
    requireLanguage(value);
    fallback = value;
}

std::vector<std::string> Catalog::getLanguages() const {
    std::vector<std::string> names;
    for (const auto& [name, entries] : tables) {
        names.push_back(name);
    }
    return names;
}

text::Direction Catalog::getDirection(std::string_view name) const {
    requireLanguage(name);
    const auto found = directions.find(name);
    return found != directions.end() ? found->second : text::Direction::LeftToRight;
}

std::pair<const core::Json*, const std::string*> Catalog::find(std::string_view key) const {
    for (const std::string* candidate : {&language, &fallback}) {
        const auto found = tables.find(*candidate);
        if (found == tables.end()) {
            continue;
        }
        const auto entry = found->second.find(key);
        if (entry != found->second.end()) {
            return {&entry->second, candidate};
        }
    }
    return {nullptr, nullptr};
}

bool Catalog::has(std::string_view key) const {
    return find(key).first != nullptr;
}

std::string Catalog::getText(std::string_view key, const core::Json& arguments) const {
    checkArguments(arguments);
    const auto [entry, entryLanguage] = find(key);
    if (entry == nullptr) {
        return std::string(key);
    }
    if (entry->is_string()) {
        const std::string& pattern = entry->get_ref<const std::string&>();
        return format(pseudo ? translatePseudo(pattern) : pattern, arguments);
    }

    // The rules of the language the text comes from pick the form, a `zero` form always takes a count of 0, and a form the entry lacks falls back to `other`.
    std::string_view form = "other";
    const auto count = arguments.find("count");
    if (count != arguments.end() && count->is_number()) {
        const double value = count->get<double>();
        const auto ordinal = arguments.find("ordinal");
        const bool ranks = ordinal != arguments.end() && ordinal->is_boolean() && ordinal->get<bool>();
        const PluralRules::Category category = ranks ? PluralRules::selectOrdinal(*entryLanguage, value) : PluralRules::select(*entryLanguage, value);
        if (value == 0.0 && !ranks && entry->contains("zero")) {
            form = "zero";
        } else if (entry->contains(PluralRules::categoryName(category))) {
            form = PluralRules::categoryName(category);
        }
    }
    const std::string& pattern = entry->at(form).get_ref<const std::string&>();
    return format(pseudo ? translatePseudo(pattern) : pattern, arguments);
}

std::optional<std::string> Catalog::findBestMatch(std::string_view tag) const {
    const std::string wanted = normalizeTag(tag);
    const std::string_view wantedBase = getBaseLanguage(wanted);
    std::optional<std::string> sameBase;
    for (const auto& [name, entries] : tables) {
        const std::string candidate = normalizeTag(name);
        if (candidate == wanted) {
            return name;
        }
        if (getBaseLanguage(candidate) == wantedBase && (!sameBase || candidate == wantedBase)) {
            sameBase = name;
        }
    }
    return sameBase;
}

} // namespace haylen::localization
