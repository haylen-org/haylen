#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::conversation {

// A small expression over the variables of a conversation, parsed once: numbers, strings in single or double quotes, `true`, `false`, `nil`, names such as `gold` or `visits.greeting`, the arithmetic `+`, `-`, `*` and `/`, the comparisons `==`, `~=`, `<`, `<=`, `>` and `>=`, and `and`, `or` and `not` with the truth of Lua, where only `false` and `nil` are false.
class Expression final {
  public:
    // Reads the value of a name, or null for a name that has none.
    using Lookup = std::function<core::Json(std::string_view name)>;

    // Throws `std::invalid_argument` with the text and the position of the mistake.
    [[nodiscard]] static Expression parse(std::string_view text);

    [[nodiscard]] core::Json evaluate(const Lookup& lookup) const;
    [[nodiscard]] bool test(const Lookup& lookup) const;

    [[nodiscard]] const std::string& getText() const noexcept {
        return text;
    }

  private:
    enum class Kind : std::uint8_t {
        Literal,
        Name,
        Not,
        Negate,
        Binary,
    };

    struct Term {
        Kind kind = Kind::Literal;
        std::string operation;
        core::Json value;
        std::size_t left = 0;
        std::size_t right = 0;
    };

    // Reads one expression from the text, keeping the position of the next character.
    class Reader final {
      public:
        Reader(std::string_view source, std::vector<Term>& output) : text(source), terms(output) {}

        [[nodiscard]] std::size_t readOr();
        [[nodiscard]] bool isDone();

      private:
        [[nodiscard]] std::size_t readAnd();
        [[nodiscard]] std::size_t readNot();
        [[nodiscard]] std::size_t readComparison();
        [[nodiscard]] std::size_t readSum();
        [[nodiscard]] std::size_t readProduct();
        [[nodiscard]] std::size_t readUnary();
        [[nodiscard]] std::size_t readPrimary();
        [[nodiscard]] std::size_t add(Term term);
        [[nodiscard]] std::size_t binary(std::string operation, std::size_t left, std::size_t right);
        [[nodiscard]] bool takeWord(std::string_view word);
        [[nodiscard]] bool takeSymbol(std::string_view symbol);
        [[nodiscard]] std::invalid_argument fail(std::string_view problem) const;
        void skipSpaces() noexcept;

        std::string_view text;
        std::vector<Term>& terms;
        std::size_t position = 0;
    };

    [[nodiscard]] core::Json evaluate(std::size_t index, const Lookup& lookup) const;
    [[nodiscard]] static bool isTrue(const core::Json& value) noexcept;
    [[nodiscard]] double number(const core::Json& value, std::string_view operation) const;

    std::vector<Term> terms;
    std::size_t root = 0;
    std::string text;
};

} // namespace haylen::conversation
