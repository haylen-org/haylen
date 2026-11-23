#include "haylen/core/JsonValidator.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace haylen::core {

void JsonValidator::requireKnownKeys(const Json& object, std::initializer_list<std::string_view> allowed, std::string_view context) {
    if (!object.is_object()) {
        throw std::invalid_argument("Expected " + std::string(context) + " to be a JSON object.");
    }

    for (const auto& [key, value] : object.items()) {
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            throw std::invalid_argument("Unknown key \"" + key + "\" in " + std::string(context) + ".");
        }
    }
}

void JsonValidator::requireKnownOptions(const Json& options, std::initializer_list<std::string_view> allowed, std::string_view context) {
    if (!options.is_object()) {
        throw std::invalid_argument("Expected " + std::string(context) + " to be a JSON object.");
    }

    for (const auto& [key, value] : options.items()) {
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            throw std::invalid_argument("Unknown option \"" + key + "\".");
        }
    }
}

Json JsonValidator::parse(std::string_view text, std::string_view file) {
    try {
        return Json::parse(text);
    } catch (const Json::parse_error& failure) {
        throw std::invalid_argument(describe(failure, text, "The file \"" + std::string(file) + "\""));
    }
}

std::string JsonValidator::describe(const Json::exception& failure, std::string_view text, std::string_view subject) {
    if (const auto* syntax = dynamic_cast<const Json::parse_error*>(&failure)) {
        return describeSyntax(syntax->byte, text, subject);
    }

    // The reader names a missing key and a value of the wrong type in its own words, from which only the key and the types are taken.
    const std::string_view message = failure.what();
    const std::string_view missing = between(message, "key '", "' not found");
    if (!missing.empty()) {
        return std::string(subject) + " lacks the key \"" + std::string(missing) + "\".";
    }

    const std::string_view needed = between(message, "type must be ", ", but is ");
    const std::string_view found = between(message, ", but is ", "");
    if (!needed.empty() && !found.empty()) {
        return std::string(subject) + " holds " + withArticle(found) + " where " + withArticle(needed) + " belongs.";
    }

    return std::string(subject) + " holds a value that is out of range or of another kind than it needs.";
}

std::string JsonValidator::describeSyntax(std::size_t position, std::string_view text, std::string_view subject) {
    // The reader counts the characters it read, so the last one it read is the mistake, unless the text ended first.
    if (position == 0 || position > text.size()) {
        return std::string(subject) + " is not valid JSON, since it ends before its last value is complete.";
    }

    const std::string_view before = text.substr(0, position - 1);
    const std::size_t line = 1 + static_cast<std::size_t>(std::ranges::count(before, '\n'));
    const std::size_t lineStart = before.rfind('\n');
    const std::size_t column = lineStart == std::string_view::npos ? position : position - 1 - lineStart;
    return std::string(subject) + " is not valid JSON. Check line " + std::to_string(line) + ", column " + std::to_string(column) + ".";
}

std::string_view JsonValidator::between(std::string_view message, std::string_view before, std::string_view after) {
    const std::size_t start = message.find(before);
    if (start == std::string_view::npos) {
        return {};
    }

    const std::string_view rest = message.substr(start + before.size());
    return after.empty() ? rest : rest.substr(0, rest.find(after));
}

std::string JsonValidator::withArticle(std::string_view typeName) {
    if (typeName == "null") {
        return "null";
    }

    const bool vowel = typeName.starts_with('a') || typeName.starts_with('o');
    return (vowel ? "an " : "a ") + std::string(typeName);
}

} // namespace haylen::core
