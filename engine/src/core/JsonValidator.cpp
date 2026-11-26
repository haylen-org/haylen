#include "haylen/core/JsonValidator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

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
    } catch (const Json::exception& failure) {
        throw std::invalid_argument(describe(failure, text, "The file \"" + std::string(file) + "\""));
    }
}

Json JsonValidator::parse(std::string_view text, std::string_view file, std::size_t maxDepth) {
    if (const std::optional<std::size_t> deep = findTooDeep(text, maxDepth)) {
        const auto [line, column] = locate(*deep, text);
        throw std::invalid_argument("The file \"" + std::string(file) + "\" nests its objects and arrays more than " + std::to_string(maxDepth) + " levels deep, more than the engine reads. Check line " + std::to_string(line) + ", column " + std::to_string(column) + ".");
    }

    return parse(text, file);
}

std::optional<std::size_t> JsonValidator::findTooDeep(std::string_view text, std::size_t maxDepth) noexcept {
    std::size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t position = 0; position < text.size(); ++position) {
        const char character = text[position];
        if (inString) {
            const bool closes = !escaped && character == '"';
            escaped = !escaped && character == '\\';
            inString = !closes;
            continue;
        }

        if (character == '"') {
            inString = true;
        } else if (character == '[' || character == '{') {
            if (++depth > maxDepth) {
                return position;
            }
        } else if ((character == ']' || character == '}') && depth > 0) {
            --depth;
        }
    }

    return std::nullopt;
}

std::pair<std::size_t, std::size_t> JsonValidator::locate(std::size_t offset, std::string_view text) noexcept {
    const std::string_view before = text.substr(0, offset);
    const std::size_t line = 1 + static_cast<std::size_t>(std::ranges::count(before, '\n'));
    const std::size_t lineStart = before.rfind('\n');
    const std::size_t column = lineStart == std::string_view::npos ? offset + 1 : offset - lineStart;
    return {line, column};
}

std::string JsonValidator::describe(const Json::exception& failure, std::string_view text, std::string_view subject) {
    if (const auto* syntax = dynamic_cast<const Json::parse_error*>(&failure)) {
        return describeSyntax(syntax->byte, text, subject);
    }

    if (const auto* range = dynamic_cast<const Json::out_of_range*>(&failure); range != nullptr && range->id == kNumberOverflow) {
        return describeOverflow(text, subject);
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

    const auto [line, column] = locate(position - 1, text);
    return std::string(subject) + " is not valid JSON. Check line " + std::to_string(line) + ", column " + std::to_string(column) + ".";
}

std::string JsonValidator::describeOverflow(std::string_view text, std::string_view subject) {
    const std::optional<std::size_t> overflow = findOverflowingNumber(text);
    if (!overflow) {
        return std::string(subject) + " holds a number too large to read.";
    }

    const auto [line, column] = locate(*overflow, text);
    return std::string(subject) + " holds a number too large to read. Check line " + std::to_string(line) + ", column " + std::to_string(column) + ".";
}

std::optional<std::size_t> JsonValidator::findOverflowingNumber(std::string_view text) {
    bool inString = false;
    bool escaped = false;
    std::size_t position = 0;
    while (position < text.size()) {
        const char character = text[position];
        if (inString) {
            const bool closes = !escaped && character == '"';
            escaped = !escaped && character == '\\';
            inString = !closes;
            ++position;
            continue;
        }

        if (character == '"') {
            inString = true;
            ++position;
            continue;
        }

        if (character != '-' && (character < '0' || character > '9')) {
            ++position;
            continue;
        }

        // The reader turns a number into a double as the standard library does, so the same conversion finds the number it refused.
        const std::size_t end = std::min(text.find_first_not_of("0123456789+-.eE", position), text.size());
        const std::string number(text.substr(position, end - position));
        if (std::isinf(std::strtod(number.c_str(), nullptr))) {
            return position;
        }

        position = end;
    }

    return std::nullopt;
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
