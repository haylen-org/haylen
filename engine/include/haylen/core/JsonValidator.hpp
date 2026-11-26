#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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

    // Parses as `parse` does, and refuses a document whose objects and arrays nest more than `maxDepth` levels deep with the line and column of the first bracket too deep, so code that walks a document from a file of anywhere never recurses without bound.
    [[nodiscard]] static Json parse(std::string_view text, std::string_view file, std::size_t maxDepth);

    // Says in plain words what a failure of the JSON reader found in what the subject names, such as `The asset "maps/cave.json" lacks the key "width".`, without the wording of the reader. A mistake of syntax and a number too large to read name their line and column in the text that was read.
    [[nodiscard]] static std::string describe(const Json::exception& failure, std::string_view text, std::string_view subject);

  private:
    static constexpr int kNumberOverflow = 406;

    [[nodiscard]] static std::string describeSyntax(std::size_t position, std::string_view text, std::string_view subject);
    [[nodiscard]] static std::string describeOverflow(std::string_view text, std::string_view subject);
    [[nodiscard]] static std::string_view between(std::string_view message, std::string_view before, std::string_view after);
    [[nodiscard]] static std::string withArticle(std::string_view typeName);

    // Returns the offset of the first bracket outside strings that opens a level past the bound.
    [[nodiscard]] static std::optional<std::size_t> findTooDeep(std::string_view text, std::size_t maxDepth) noexcept;

    // Returns the offset of the first number outside strings that is too large for a double.
    [[nodiscard]] static std::optional<std::size_t> findOverflowingNumber(std::string_view text);

    // Returns the line and the column, both counted from one, of a byte of the text.
    [[nodiscard]] static std::pair<std::size_t, std::size_t> locate(std::size_t offset, std::string_view text) noexcept;
};

} // namespace haylen::core
