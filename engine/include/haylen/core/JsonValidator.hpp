#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"

namespace haylen::core {

// Checks the shape of the JSON documents and option objects that configure the engine.
class JsonValidator final {
  public:
    // Throws `std::invalid_argument` naming the first key of the object that is not allowed, so a misspelled setting never goes unnoticed.
    static void requireKnownKeys(const Json& object, std::initializer_list<std::string_view> allowed, std::string_view context);

    // Throws `std::invalid_argument` with `Unknown option "<key>".` for the first option that is not allowed, the message every option table of the engine raises, and names the options in the message for a value that is not an object.
    static void requireKnownOptions(const Json& options, std::initializer_list<std::string_view> allowed, std::string_view context);

    // Parses the text of a JSON file of the app, such as `haylen.json` or a map, and throws `std::invalid_argument` with a sentence that names the file and the line and column of its first mistake.
    [[nodiscard]] static Json parse(std::string_view text, std::string_view file);

    // Says in plain words what a failure of the JSON reader found in what the subject names, such as `The asset "maps/cave.json" lacks the key "width".`, without the wording of the reader. A mistake of syntax names its line and column in the text that was read.
    [[nodiscard]] static std::string describe(const Json::exception& failure, std::string_view text, std::string_view subject);

  private:
    [[nodiscard]] static std::string describeSyntax(std::size_t position, std::string_view text, std::string_view subject);
    [[nodiscard]] static std::string_view between(std::string_view message, std::string_view before, std::string_view after);
    [[nodiscard]] static std::string withArticle(std::string_view typeName);
};

} // namespace haylen::core
